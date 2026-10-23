"""Execute the production sensor/SOC code offline; time mocked-pin ISR brackets.

No probes or buffers are linked into production firmware. Clock counts exclude
hardware interrupt entry, nested interrupts and the BBS shared timer wrapper.
"""
import argparse
import os
from pathlib import Path
import pty
import re
import select
import subprocess
import tempfile
import time

HARNESS = r'''
#include "battery.c"
static uint32_t now;
static uint16_t volts = 540, amps;
static uint8_t demand;
uint32_t system_ms(void) { return now; }
uint16_t motor_get_battery_voltage_x10(void) { return volts; }
uint16_t motor_get_battery_current_x10(void) { return amps; }
uint8_t motor_get_target_current(void) { return demand; }
volatile uint8_t failure_code, timing_case;
void passed(void) { failure_code = 0; for (;;) {} }
void failed(void) { for (;;) {} }
void probe_begin(void) {}
void timing_begin(void) {}
void timing_end(void) {}
#define CHECK(x, code) do { if (!(x)) { failure_code = code; failed(); } } while (0)
void main(void)
{
    sensors_init();
    probe_begin();
    for (uint8_t i = 0; i < 5; ++i)
    {
        pedal_sample(false, false, 99); pedal_sample(true, false, 1);
    }
    CHECK(pas_is_pedaling_forwards() && pas_period_length == 99, 1);
    uint16_t count = pas_get_pulse_counter();
    pedal_sample(false, false, 19); pedal_sample(true, true, 1);
#ifndef BASELINE_PROBE
    CHECK(pas_get_pulse_counter() == count && pas_period_length == 99, 2);
    CHECK(!pas_is_pedaling_backwards(), 3);
#endif
    pedal_sample(false, true, 99); pedal_sample(true, true, 1);
    CHECK(pas_is_pedaling_backwards(), 4);
#ifndef BASELINE_PROBE
    CHECK(pas_get_pulse_counter() == 0, 14);
#endif
    pedal_sample(false, false, 65535); pedal_sample(false, false, 100);
    CHECK(pas_period_counter == 65535 && pas_get_cadence_rpm_x10() == 0, 5);
#if !defined(TSDZ2)
    g_config.use_temperature_sensor = TEMPERATURE_SENSOR_CONTR | TEMPERATURE_SENSOR_MOTOR;
    CHECK(temperature_contr_x100() > 2400 && temperature_contr_x100() < 2600, 6);
#if HAS_MOTOR_TEMP_SENSOR
    CHECK(temperature_motor_x100() > 2400 && temperature_motor_x100() < 2600, 7);
#endif
#ifndef BASELINE_PROBE
    controller_adc = 1023; motor_adc = 0;
    CHECK(temperature_contr_x100() == TEMPERATURE_SENSOR_FAULT_X100, 8);
#if HAS_MOTOR_TEMP_SENSOR
    CHECK(temperature_motor_x100() == TEMPERATURE_SENSOR_FAULT_X100, 9);
#endif
    g_config.use_temperature_sensor = 0;
    CHECK(temperature_contr_x100() == 0 && temperature_motor_x100() == 0, 10);
#endif
#else
    CHECK(temperature_contr_x100() == 0 && temperature_motor_x100() == 0, 11);
#endif
    g_config.max_battery_x100v_u16h = 5880u >> 8;
    g_config.max_battery_x100v_u16l = (uint8_t)5880u;
    g_config.low_cut_off_v = 42;
    battery_init(); battery_process();
    uint8_t initial = battery_get_percent();
    demand = 50; amps = 100; volts = 480;
    for (uint16_t frame = 0; frame < 2003; ++frame) { now += 5; battery_process(); }
#ifndef BASELINE_PROBE
    CHECK(battery_get_percent() == initial - 1u, 12);
#endif
    CHECK(battery_get_percent() <= 100, 13);
    for (timing_case = 0; timing_case < 6; ++timing_case)
    {
        pas_prev1 = false; pas_prev2 = false;
        pas_period_length = 100; pas_stop_delay_periods = 1500;
        pas_direction_backward = false; pas_pulse_counter = 10;
        pas_period_counter = timing_case == 2 ? 10 : timing_case >= 4 ? 65535 : 99;
        PIN_VALUE(PIN_PAS1) = timing_case > 0 && timing_case < 4;
        PIN_VALUE(PIN_PAS2) = timing_case == 3;
        speed_period_counter = 600; speed_ticks_period_length = 1000;
        speed_prev_state = false;
        PIN_VALUE(PIN_SPEED_SENSOR) = timing_case == 1;
#ifndef TSDZ2
        speed_state_counter = 9;
#endif
        timing_begin();
        SENSOR_TICK();
        timing_end();
    }
    passed();
}
'''


def verify(controller, sdcc, simulator, baseline=None, assembly=None):
    root = Path(__file__).resolve().parents[1]
    fixture = (root / 'tests/sensor_fixture.h').read_text()
    fixture = fixture.replace('"../src/firmware/', '"' + str(root / 'src/firmware') + '/')
    with tempfile.TemporaryDirectory() as temporary:
        work = Path(temporary)
        harness = fixture + HARNESS
        if baseline:
            (work / 'sensors.h').write_bytes(subprocess.check_output(['git', 'show', baseline + ':src/firmware/sensors.h'], cwd=root))
            for relative in ('battery.c', 'bbsx/sensors.c', 'tsdz2/sensors.c'):
                path = work / relative.replace('/', '-')
                path.write_bytes(subprocess.check_output(['git', 'show', baseline + ':src/firmware/' + relative], cwd=root))
                original = '"' + (str(root / 'src/firmware' / relative) if relative != 'battery.c' else relative) + '"'
                harness = harness.replace(original, '"' + str(path) + '"')
            harness = '#define BASELINE_PROBE 1\n' + harness
        source = work / 'sensor.c'
        source.write_text(harness)
        flags = ['-mstm8'] if controller == 'TSDZ2' else ['-mmcs51', '--model-large']
        compiled = subprocess.run([sdcc, *flags, '--std-c99', '-D' + controller, '-I' + str(root / 'src/firmware'),
                                   str(source), '-o', str(work / 'sensor.ihx')], capture_output=True, text=True)
        if compiled.returncode: raise RuntimeError(compiled.stdout + compiled.stderr)
        symbols = (work / 'sensor.map').read_text()
        names = ('passed', 'failed', 'probe_begin', 'timing_begin', 'timing_end', 'failure_code')
        addresses = {name: int(re.search(r'([0-9A-F]+)\s+_' + name + r'\s', symbols)[1], 16) for name in names}
        master, slave = pty.openpty()
        process = subprocess.Popen([simulator, '-t', 'STM8S105' if controller == 'TSDZ2' else 'C52', '-q', '-c', '-',
                                    str(work / 'sensor.ihx')], stdin=slave, stdout=slave, stderr=slave)
        def receive(timeout=45, startup=False):
            output = b''
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                if select.select([master], [], [], 0.1)[0]: output += os.read(master, 65536)
                if (startup and b'0> ' in output) or re.search(rb'0>\s*$', output): return output.decode(errors='replace')
                if process.poll() is not None: raise RuntimeError(output.decode(errors='replace'))
            raise TimeoutError(output.decode(errors='replace'))
        def command(value):
            os.write(master, (value + '\n').encode())
            return receive()
        def cycles():
            return int(re.search(r'Total time[^\n]*\((\d+) clks\)', command('state'))[1])
        def run_to(name):
            output = command('run')
            stop = re.search(r'Stop at 0x([0-9a-fA-F]+)', output)
            if not stop or int(stop[1], 16) != addresses[name]:
                memory = 'rom' if controller == 'TSDZ2' else 'xram'
                output += command(f"dump {memory} 0x{addresses['failure_code']:x} 0x{addresses['failure_code']:x}")
                raise AssertionError(output)
            return output
        try:
            receive(startup=True)
            if controller == 'TSDZ2':
                command('expression SP=0x7ff'); command('set memory cpu_0_cfg 0 0x3ff')
            for name in ('probe_begin', 'timing_begin', 'timing_end', 'passed', 'failed'):
                command(f'break 0x{addresses[name]:x}')
            start = run_to('probe_begin')
            sp = int(re.search(r'SP\s*=?\s*0x([0-9a-fA-F]+)', start)[1], 16)
            low, high = (0x400, sp) if controller == 'TSDZ2' else (sp + 1, 0xff)
            memory = 'rom' if controller == 'TSDZ2' else 'iram'
            command(f'fill {memory} 0x{low:x} 0x{high:x} 0xa5')
            brackets = []
            for case in range(6):
                run_to('timing_begin'); before = cycles()
                run_to('timing_end'); brackets.append(cycles() - before)
            run_to('passed')
            dump = command(f'dump {memory} 0x{low:x} 0x{high:x}')
            touched = []
            for line in dump.splitlines():
                match = re.match(r'0x([0-9a-fA-F]+)\s+((?:[0-9a-fA-F]{2}\s+)+)', line)
                if match:
                    address = int(match[1], 16)
                    touched.extend(address + offset for offset, value in enumerate(match[2].split()) if value.lower() != 'a5')
            watermark = (sp - min(touched) + 1 if controller == 'TSDZ2' else max(touched) - sp) if touched else 0
            if watermark >= high - low - 16: raise AssertionError('Probe exhausted its stack watermark region')
            # These bound the mocked-pin regression workload, not real interrupt latency.
            if max(brackets) > (128 if controller == 'TSDZ2' else 2880):
                raise AssertionError('ISR bracket exceeded its reviewed simulator regression budget')
            print(f'{controller} ({baseline or "working tree"}): sensor/SOC MCU checks: PASS; '
                  f'probe stack extension {watermark} B; ISR brackets {brackets} simulator clocks')
        finally:
            process.terminate(); process.wait(timeout=10)
            os.close(master); os.close(slave)
    if assembly:
        label = '_isr_timer4_ovf:' if controller == 'TSDZ2' else '_sensors_timer0_isr:'
        body = assembly.read_text().split(label, 1)[1]
        if re.search(r'^\s*(?:lcall|acall|call|callf|mul|div|divw)\s', body, re.M):
            raise AssertionError('Production ISR contains a helper call or multiply/divide instruction')
        print(controller + ': production ISR has no helper calls or multiply/divide instructions: PASS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--controller', choices=('BBS02', 'BBSHD', 'TSDZ2'), required=True)
    parser.add_argument('--sdcc', default='sdcc')
    parser.add_argument('--simulator')
    parser.add_argument('--baseline-ref')
    parser.add_argument('--assembly', type=Path)
    args = parser.parse_args()
    verify(args.controller, args.sdcc, args.simulator or ('ucsim_stm8' if args.controller == 'TSDZ2' else 'ucsim_51'),
           args.baseline_ref, args.assembly)

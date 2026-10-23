# Végleges kutatási összefoglaló — BBS02B / BBSHD / TSDZ2

Ez a csomag **pontosan három** fejlesztési célprofilt tartalmaz. A forrásmotor neve eltérhet a célhardvertől: a bizonyítékot a `source_id`, `evidence_type`, `motor_family`, `target_profile` és `calibration_status` azonosítja. A cél nem egy hangzatos %-os hatásfokjavulás, hanem valós Wh/km megtakarítás azonos menetviszonyok és emberi pedálteljesítmény mellett.

## BBS02B — 93 sor gyári mérési referencia
A `BBS750_FACTORY` 48 V/750 W-os gyári load-sweep 93 közölt pontját a felhasználó a BBS02B 750 W fejlesztéshez elfogadta. A PDF nem jelzi a pontos gyártási/vezérlő-revíziót. A BBS02B-profilhoz ezért **provisional engineering proxy** jelöléssel szerepel, egyetlen, új 2D adatokat nem tartalmazó adatsorként. A maximális közölt η 85,8% 35,1 Nm, 117,0 rpm és ~501,29 W elektromos bemenet mellett; az egyik nagyterhelésű végpont 90,4 Nm, 92,5 rpm, 1157,63 W bemenet, 875,69 W mechanikai kimenet, 75,6% η. Ez `Pin-Pout=281,94 W` összveszteséget jelent a konkrét pontban, nem külön megmért rézveszteséget. Ugyanezeket külön BBS02 adatsorként nem szállítjuk. Egy korábbi motorcsalád Grin 2019 paraméterei a BBS02B profilban csak `related_family_reference_only` címkével használhatók.

## BBSHD — gyári 4 referenciapont és önálló Grin-adatok
`BBSHD_FACTORY`: 2016 48 V/1000 W karakterisztika, négy numerikus felirat. Az η legnagyobb gyári megjelölt pontja 85,6%, 29,4 Nm, 156,4 rpm, 481,53 W Pout, 11,75 A. A legnagyobb kimenő teljesítményre jelzett pont 1077,82 W, 87 Nm, 118,3 rpm. A hiányzó hatásfokértékeket nem töltjük ki látszólagos méréssel. `GRIN_2019`: kimenő noload-Kv ~3,27 crank-rpm/V, feketedoboz ellenállás 0,086 Ω, 48 mm állórész, 1,80 kg állórész+ház, 8 forgórészmágnes. Ezek mértpéldány-paraméterek, nem minden BBSHD motor univerzális konstansai.

## TSDZ2 — saját motorparaméter-forrás + külső B-verziós összehasonlítás
`TSDZ2_V2_MOTOR` nyílt motorvezérlő: 36 V R=0,094 Ω, L=76 µH; 48 V R=0,125 Ω, L=135 µH; 8 póluspár; rotor üresjárat ~4000 rpm. Fázis/vezeték mérési konvenció és használható valódi üzemi, hőmérsékletfüggő érték nincs azonosítva. A tényleges platform STM8 motorkódja hozzáfér a motorvezérlési részekhez, de ezen forrásból **nem** következik automatikus FOC átállítás.

`TSDZ2B_BENCH`: TSDZ2B mintán végzett 9 közelítő, tréner-alapú firmware/hűtés összehasonlítás. 60 rpm, hűtés nélkül stock~331 W/η65%, OSF~353 W/η70%; 90 rpm stock~328 W/η64%, OSF~370 W/η73%. Más firmware, más motorváltozat és részben eltérő akkufeszültség. A hűtéses sorok növekvő 70°C elérési ideje **nem** bizonyítja a firmware önmagában növelt η-ját. A 70 °C kizárólag a vizsgálat mérföldköve, nem átemelhető biztonsági hőlimit.

## Mi következik firmware szinten?
1. Első a teljes cikluson át **mért pack V/A** és pontos kerékút adatból számított Wh/km, érvényességjelzővel; külső mérővel validálva.
2. PR #9 Adaptive Power PAS céláramját safety pipeline UTÁN semelyik komfort-rámpa nem emelheti a korlát fölé. A mozgó célérték S-görbe beragadását teszteljük és javítjuk, a legacy PAS karakterisztika változtatása nélkül.
3. Terhelés/kadenciafüggő, **alapból OFF** soft korlátozó csak mérhető adatokkal és hiszterézissel; nem állítjuk, hogy valódi instant motor-η optimumot követünk.
4. BBS02B és BBSHD: közvetlen NEC phase current/FOC nem vezérelhető az STC PAS algoritmusból. TSDZ2: FOC csak külön, mérőpados kutatás után.
5. Számszerű megtakarítást kizárólag valós, ismételt ABBA próbamenetekből jelentünk, azonos sebesség és lehetőleg rider W mellett.

## Források
A `data/sources.csv` tartalmazza az összes megőrzött kanonikus hivatkozást; a profil CSV-k `source_id` mezője ezen táblához kapcsolódik. Gyári dokumentumokat teljes terjedelemben nem másoltunk a ZIP-be. A nyers gyári PDF URL-ek a BBS750_FACTORY és BBSHD_FACTORY rekordoknál vannak.

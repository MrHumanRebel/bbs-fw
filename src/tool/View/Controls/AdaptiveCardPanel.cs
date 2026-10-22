using System;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace BBSFW.View.Controls
{
    /// <summary>Fits cards into the available width, balancing naturally measured column heights.</summary>
    public sealed class AdaptiveCardPanel : Panel
    {
        public static readonly DependencyProperty MinimumCardWidthProperty = DependencyProperty.Register(
            nameof(MinimumCardWidth), typeof(double), typeof(AdaptiveCardPanel),
            new FrameworkPropertyMetadata(310d, FrameworkPropertyMetadataOptions.AffectsMeasure));
        public double MinimumCardWidth { get => (double)GetValue(MinimumCardWidthProperty); set => SetValue(MinimumCardWidthProperty, value); }
        private const double Gap = 16;
        private Rect[] positions = Array.Empty<Rect>();
        protected override Size MeasureOverride(Size availableSize)
        {
            double width = double.IsInfinity(availableSize.Width) ? MinimumCardWidth : Math.Max(0, availableSize.Width);
            int columns = Math.Max(1, (int)((width + Gap) / (Math.Max(1, MinimumCardWidth) + Gap)));
            columns = Math.Min(columns, Math.Max(1, InternalChildren.Cast<UIElement>().Count(c => c.Visibility != Visibility.Collapsed)));
            double cardWidth = Math.Max(0, (width - (columns - 1) * Gap) / columns);
            var heights = new double[columns];
            positions = new Rect[InternalChildren.Count];
            for (int i = 0; i < InternalChildren.Count; ++i)
            {
                var child = InternalChildren[i];
                if (child.Visibility == Visibility.Collapsed) continue;
                int column = Array.IndexOf(heights, heights.Min());
                child.Measure(new Size(cardWidth, double.PositiveInfinity));
                positions[i] = new Rect(column * (cardWidth + Gap), heights[column], cardWidth, child.DesiredSize.Height);
                heights[column] += child.DesiredSize.Height + Gap;
            }
            return new Size(width, Math.Max(0, heights.Max() - Gap));
        }
        protected override Size ArrangeOverride(Size finalSize)
        {
            for (int i = 0; i < InternalChildren.Count; ++i) InternalChildren[i].Arrange(positions[i]);
            return finalSize;
        }
    }
}

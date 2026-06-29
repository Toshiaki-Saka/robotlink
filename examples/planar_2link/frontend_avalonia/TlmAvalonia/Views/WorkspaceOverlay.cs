// Views/WorkspaceOverlay.cs — renders boundary curves directly via DrawingContext.

using System.Collections.Generic;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using TlmAvalonia.Models;

namespace TlmAvalonia.Views;

public sealed class WorkspaceOverlay : Control
{
    public static readonly StyledProperty<IReadOnlyList<BoundaryPolyline>?> SegmentsProperty =
        AvaloniaProperty.Register<WorkspaceOverlay, IReadOnlyList<BoundaryPolyline>?>(nameof(Segments));

    // BoundsProperty を AffectsRender に含めないとリサイズで再描画されない (AVALONIA_NOTES.md §2)
    static WorkspaceOverlay()
    {
        AffectsRender<WorkspaceOverlay>(SegmentsProperty, BoundsProperty);
    }

    public IReadOnlyList<BoundaryPolyline>? Segments
    {
        get => GetValue(SegmentsProperty);
        set => SetValue(SegmentsProperty, value);
    }

    public override void Render(DrawingContext context)
    {
        var segments = Segments;
        if (segments is null) return;
        IBrush? lastBrush = null;
        Pen? pen = null;
        foreach (var seg in segments)
        {
            if (!ReferenceEquals(seg.LineBrush, lastBrush))
            {
                pen = new Pen(seg.LineBrush, 1.5);
                lastBrush = seg.LineBrush;
            }
            context.DrawLine(pen!, seg.Start, seg.End);
        }
    }
}

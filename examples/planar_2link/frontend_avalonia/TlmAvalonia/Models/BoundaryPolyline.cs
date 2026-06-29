// Models/BoundaryPolyline.cs — one line segment of a workspace boundary curve.

using Avalonia;
using Avalonia.Media;

namespace TlmAvalonia.Models;

public sealed class BoundaryPolyline
{
    public required Point  Start     { get; init; }
    public required Point  End       { get; init; }
    public required IBrush LineBrush { get; init; }
}

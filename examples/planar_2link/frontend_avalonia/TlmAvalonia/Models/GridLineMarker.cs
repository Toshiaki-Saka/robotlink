// Models/GridLineMarker.cs — render DTO for explicit grid lines.

using Avalonia;

namespace TlmAvalonia.Models;

public sealed class GridLineMarker
{
    public Point Start { get; init; }
    public Point End   { get; init; }
}

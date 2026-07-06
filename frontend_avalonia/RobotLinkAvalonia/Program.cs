using Avalonia;
using RobotLinkAvalonia.Views;

namespace RobotLinkAvalonia;

internal sealed class Program
{
    // Set before Avalonia initialises; read from App.OnFrameworkInitializationCompleted.
    internal static string? InitialCsvPath;

    [STAThread]
    public static void Main(string[] args)
    {
        // Resolve CSV path before the Avalonia runtime boots so that
        // OnFrameworkInitializationCompleted can hand it to MainWindow.
        InitialCsvPath = args.Length > 0 ? args[0] : FindDefaultCsv();
        BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
    }

    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
                     .UsePlatformDetect()
                     .WithInterFont()
                     .LogToTrace();

    private static string? FindDefaultCsv()
    {
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        for (int i = 0; i < 7; i++)
        {
            var candidate = Path.Combine(dir.FullName, "output", "sim_results.csv");
            if (File.Exists(candidate)) return candidate;
            if (dir.Parent is null) break;
            dir = dir.Parent;
        }
        return null;
    }
}

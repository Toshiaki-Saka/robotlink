using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Platform.Storage;
using RobotLinkAvalonia.ViewModels;
using System.ComponentModel;
using System.Diagnostics;

namespace RobotLinkAvalonia.Views;

public partial class MainWindow : Window
{
    private readonly MainWindowViewModel _vm = new();
    private ArmAnimControl? _animControl;

    public MainWindow() : this(null) { }

    public MainWindow(string? csvPath)
    {
        InitializeComponent();
        DataContext = _vm;
        _animControl = this.FindControl<ArmAnimControl>("AnimControl");
        _vm.PropertyChanged += OnVmPropertyChanged;

        if (!string.IsNullOrEmpty(csvPath) && File.Exists(csvPath))
            _vm.LoadCsv(csvPath!);
    }

    private void OnVmPropertyChanged(object? sender, PropertyChangedEventArgs e)
    {
        if (e.PropertyName == nameof(MainWindowViewModel.ArmFrames) && _animControl is not null)
        {
            _animControl.SetData(_vm.ArmFrames);
            _animControl.Play();
        }
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);

    // ── Button handlers ───────────────────────────────────────────────────
    public void OnPlotAll(object? sender, RoutedEventArgs e)
    {
        // Re-render all charts from the currently loaded data
        _vm.RefreshCharts();
    }

    public void OnPlayAnimation(object? sender, RoutedEventArgs e)
    {
        _animControl?.Play();
    }

    public void OnStopAnimation(object? sender, RoutedEventArgs e)
    {
        _animControl?.Stop();
    }

    public void OnClose(object? sender, RoutedEventArgs e) => Close();

    public async void OnOpenCsv(object? sender, RoutedEventArgs e)
    {
        var opts = new FilePickerOpenOptions
        {
            Title = "Open simulation results CSV",
            AllowMultiple = false,
            FileTypeFilter = new[]
            {
                new FilePickerFileType("CSV files") { Patterns = new[] { "*.csv" } },
                new FilePickerFileType("All files") { Patterns = new[] { "*.*"  } },
            }
        };

        var files = await StorageProvider.OpenFilePickerAsync(opts);
        if (files.Count > 0)
            _vm.LoadCsv(files[0].Path.LocalPath);
    }

    public async void OnRunSim(object? sender, RoutedEventArgs e)
    {
        // Search for robot_sim.exe going up from the executable directory
        var dir = new DirectoryInfo(AppContext.BaseDirectory);
        string? simExe = null;
        for (int i = 0; i < 7 && dir is not null; i++)
        {
            var candidates = new[]
            {
                Path.Combine(dir.FullName, "build", "Release", "robot_sim.exe"),
                Path.Combine(dir.FullName, "build", "robot_sim.exe"),
            };
            simExe = candidates.FirstOrDefault(File.Exists);
            if (simExe is not null) break;
            dir = dir.Parent;
        }

        if (simExe is null)
        {
            _vm.StatusText = "robot_sim.exe が見つかりません。先にビルドしてください。";
            return;
        }

        _vm.StatusText = "シミュレーションを実行中…";

        var outDir = Path.Combine(Path.GetDirectoryName(simExe)!, "..", "..", "output");
        outDir = Path.GetFullPath(outDir);
        Directory.CreateDirectory(outDir);

        var psi = new ProcessStartInfo(simExe, $"\"{outDir}\"")
        {
            RedirectStandardError = true,
            UseShellExecute = false,
        };

        try
        {
            using var proc = Process.Start(psi)!;
            await proc.WaitForExitAsync();
            if (proc.ExitCode != 0)
            {
                var err = await proc.StandardError.ReadToEndAsync();
                _vm.StatusText = $"シミュレーション失敗: {err[..Math.Min(err.Length, 200)]}";
                return;
            }
        }
        catch (Exception ex)
        {
            _vm.StatusText = $"起動失敗: {ex.Message}";
            return;
        }

        var csvPath = Path.Combine(outDir, "sim_results.csv");
        if (File.Exists(csvPath))
            _vm.LoadCsv(csvPath);
        else
            _vm.StatusText = $"CSV が生成されませんでした: {csvPath}";
    }
}

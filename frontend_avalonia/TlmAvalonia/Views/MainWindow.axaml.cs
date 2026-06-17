using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;

using TlmAvalonia.ViewModels;

namespace TlmAvalonia.Views;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
    }

    private void InitializeComponent() => AvaloniaXamlLoader.Load(this);

    private MainWindowViewModel? Vm => DataContext as MainWindowViewModel;

    public void OnResetClick(object? sender, RoutedEventArgs e) => Vm?.ResetAll();
}

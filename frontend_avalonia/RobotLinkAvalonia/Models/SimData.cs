// Models/SimData.cs — parses output/sim_results.csv into typed arrays.

namespace RobotLinkAvalonia.Models;

public sealed class SimData
{
    public double[] T   { get; private set; } = Array.Empty<double>();
    public double[] Q1  { get; private set; } = Array.Empty<double>();
    public double[] Q2  { get; private set; } = Array.Empty<double>();
    public double[] Q3  { get; private set; } = Array.Empty<double>();
    public double[] Qd1 { get; private set; } = Array.Empty<double>();
    public double[] Qd2 { get; private set; } = Array.Empty<double>();
    public double[] Qd3 { get; private set; } = Array.Empty<double>();
    public double[] Tau1 { get; private set; } = Array.Empty<double>();
    public double[] Tau2 { get; private set; } = Array.Empty<double>();
    public double[] Tau3 { get; private set; } = Array.Empty<double>();
    public double[] Err1 { get; private set; } = Array.Empty<double>();
    public double[] Err2 { get; private set; } = Array.Empty<double>();
    public double[] Err3 { get; private set; } = Array.Empty<double>();
    public double[] HandX    { get; private set; } = Array.Empty<double>();
    public double[] HandY    { get; private set; } = Array.Empty<double>();
    public double[] HandZ    { get; private set; } = Array.Empty<double>();
    public double[] HandDesX { get; private set; } = Array.Empty<double>();
    public double[] HandDesY { get; private set; } = Array.Empty<double>();
    public double[] HandDesZ { get; private set; } = Array.Empty<double>();

    public int Count => T.Length;

    public double[] HandError()
    {
        var result = new double[Count];
        for (int i = 0; i < Count; i++)
        {
            double dx = HandX[i] - HandDesX[i];
            double dy = HandY[i] - HandDesY[i];
            double dz = HandZ[i] - HandDesZ[i];
            result[i] = Math.Sqrt(dx * dx + dy * dy + dz * dz) * 1000.0; // mm
        }
        return result;
    }

    public static double[] ToDeg(double[] rad)
    {
        const double R = 180.0 / Math.PI;
        var result = new double[rad.Length];
        for (int i = 0; i < rad.Length; i++) result[i] = rad[i] * R;
        return result;
    }

    public static SimData Load(string path)
    {
        var lines = File.ReadAllLines(path);
        if (lines.Length < 2)
            throw new InvalidDataException("CSV has no data rows.");

        // Parse header
        var headers = lines[0].Split(',');
        var col = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
        for (int i = 0; i < headers.Length; i++)
            col[headers[i].Trim()] = i;

        int n = lines.Length - 1;

        var t    = new double[n]; var q1   = new double[n]; var q2  = new double[n]; var q3  = new double[n];
        var qd1  = new double[n]; var qd2  = new double[n]; var qd3 = new double[n];
        var tau1 = new double[n]; var tau2 = new double[n]; var tau3 = new double[n];
        var err1 = new double[n]; var err2 = new double[n]; var err3 = new double[n];
        var hx   = new double[n]; var hy   = new double[n]; var hz  = new double[n];
        var hdx  = new double[n]; var hdy  = new double[n]; var hdz = new double[n];

        int row = 0;
        for (int li = 1; li < lines.Length; li++)
        {
            var parts = lines[li].Split(',');
            if (parts.Length == 0 || string.IsNullOrWhiteSpace(parts[0])) continue;
            if (row >= n) break;

            double G(string name) =>
                col.TryGetValue(name, out int idx) && idx < parts.Length
                    && double.TryParse(parts[idx].Trim(),
                        System.Globalization.NumberStyles.Float,
                        System.Globalization.CultureInfo.InvariantCulture, out double v)
                    ? v : 0.0;

            t[row]    = G("t");
            q1[row]   = G("q1");   q2[row]  = G("q2");  q3[row]  = G("q3");
            qd1[row]  = G("qd1");  qd2[row] = G("qd2"); qd3[row] = G("qd3");
            tau1[row] = G("tau1"); tau2[row] = G("tau2"); tau3[row] = G("tau3");
            err1[row] = G("err1"); err2[row] = G("err2"); err3[row] = G("err3");
            hx[row]   = G("hand_x");     hy[row]  = G("hand_y");     hz[row]  = G("hand_z");
            hdx[row]  = G("hand_des_x"); hdy[row] = G("hand_des_y"); hdz[row] = G("hand_des_z");
            row++;
        }

        // Trim to actual row count
        if (row < n)
        {
            t    = t[..row];    q1 = q1[..row];  q2 = q2[..row];  q3 = q3[..row];
            qd1  = qd1[..row];  qd2 = qd2[..row]; qd3 = qd3[..row];
            tau1 = tau1[..row]; tau2 = tau2[..row]; tau3 = tau3[..row];
            err1 = err1[..row]; err2 = err2[..row]; err3 = err3[..row];
            hx   = hx[..row];   hy = hy[..row];  hz = hz[..row];
            hdx  = hdx[..row];  hdy = hdy[..row]; hdz = hdz[..row];
        }

        if (row == 0) throw new InvalidDataException("No data rows found in CSV.");

        return new SimData
        {
            T    = t,    Q1 = q1,   Q2 = q2,   Q3 = q3,
            Qd1  = qd1,  Qd2 = qd2, Qd3 = qd3,
            Tau1 = tau1, Tau2 = tau2, Tau3 = tau3,
            Err1 = err1, Err2 = err2, Err3 = err3,
            HandX = hx,   HandY = hy,   HandZ = hz,
            HandDesX = hdx, HandDesY = hdy, HandDesZ = hdz,
        };
    }
}

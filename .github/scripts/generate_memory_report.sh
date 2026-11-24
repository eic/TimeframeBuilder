#!/bin/bash
# Generate HTML report for memory usage analysis
# Usage: generate_memory_report.sh <memory_report_dir>

REPORT_DIR=$1

if [ -z "$REPORT_DIR" ]; then
    echo "Usage: $0 <memory_report_dir>"
    exit 1
fi

if [ ! -d "$REPORT_DIR" ]; then
    echo "Error: Directory $REPORT_DIR does not exist"
    exit 1
fi

# Generate HTML report for memory usage
cat > "$REPORT_DIR/index.html" << 'EOFHTML'
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Memory Usage Report</title>
    <script src="https://cdn.plot.ly/plotly-2.27.0.min.js"></script>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
               max-width: 1400px; margin: 40px auto; padding: 0 20px; line-height: 1.6; }
        h1 { color: #2c3e50; border-bottom: 3px solid #3498db; padding-bottom: 10px; }
        h2 { color: #34495e; margin-top: 30px; }
        .chart { margin: 30px 0; border: 1px solid #ddd; border-radius: 5px; padding: 15px; }
        .stats { background: #f8f9fa; padding: 15px; border-radius: 5px; margin: 20px 0; }
        .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
        .stat-item { background: white; padding: 15px; border-radius: 5px; border-left: 4px solid #3498db; }
        .stat-label { font-size: 0.9em; color: #7f8c8d; }
        .stat-value { font-size: 1.5em; font-weight: bold; color: #2c3e50; }
    </style>
</head>
<body>
    <h1>Memory Usage Report</h1>
    <p>Memory consumption over time for different pipeline stages</p>
EOFHTML

# Process each CSV file and add to HTML
for csv_file in "$REPORT_DIR"/*.csv; do
    if [ -f "$csv_file" ]; then
        filename=$(basename "$csv_file" .csv)
        # Convert underscores to spaces for display
        display_name="${filename//_/ }"

        cat >> "$REPORT_DIR/index.html" << EOFHTML
    <h2>$display_name</h2>
    <div class="stats">
        <div class="stats-grid" id="stats-${filename}"></div>
    </div>
    <div class="chart" id="chart-${filename}"></div>
    <script>
    fetch('${filename}.csv')
        .then(response => response.text())
        .then(data => {
            const rows = data.trim().split('\\n').slice(1);
            const times = [], rss = [], vms = [];
            rows.forEach(row => {
                const [t, r, v] = row.split(',').map(Number);
                if (!isNaN(t)) {
                    times.push(t);
                    rss.push(r);
                    vms.push(v);
                }
            });

            const maxRss = Math.max(...rss);
            const avgRss = rss.reduce((a,b) => a+b, 0) / rss.length;
            const duration = Math.max(...times);

            document.getElementById('stats-${filename}').innerHTML = \`
                <div class="stat-item">
                    <div class="stat-label">Peak RSS</div>
                    <div class="stat-value">\${maxRss.toFixed(0)} MB</div>
                </div>
                <div class="stat-item">
                    <div class="stat-label">Avg RSS</div>
                    <div class="stat-value">\${avgRss.toFixed(0)} MB</div>
                </div>
                <div class="stat-item">
                    <div class="stat-label">Duration</div>
                    <div class="stat-value">\${duration} s</div>
                </div>
            \`;

            const trace1 = {
                x: times, y: rss,
                name: 'RSS (Resident Set Size)',
                type: 'scatter',
                mode: 'lines',
                line: { color: '#3498db', width: 2 }
            };
            const trace2 = {
                x: times, y: vms,
                name: 'VMS (Virtual Memory Size)',
                type: 'scatter',
                mode: 'lines',
                line: { color: '#e74c3c', width: 2 }
            };
            const layout = {
                xaxis: { title: 'Time (seconds)' },
                yaxis: { title: 'Memory (MB)' },
                hovermode: 'x unified'
            };
            Plotly.newPlot('chart-${filename}', [trace1, trace2], layout);
        });
    </script>
EOFHTML
    fi
done

cat >> "$REPORT_DIR/index.html" << 'EOFHTML'
</body>
</html>
EOFHTML

echo "HTML report generated: $REPORT_DIR/index.html"

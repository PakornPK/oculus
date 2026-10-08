/**
 * Chart.js helper utilities for Oculus ROM visualization.
 */

const CHART_COLORS = {
    left: 'rgba(56, 189, 248, 1)',
    leftFill: 'rgba(56, 189, 248, 0.15)',
    right: 'rgba(251, 146, 60, 1)',
    rightFill: 'rgba(251, 146, 60, 0.15)',
    grid: 'rgba(71, 85, 105, 0.4)',
    text: 'rgba(148, 163, 184, 1)',
};

const CHART_DEFAULTS = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
        legend: {
            labels: { color: CHART_COLORS.text, font: { size: 12 } },
        },
    },
    scales: {
        x: {
            ticks: { color: CHART_COLORS.text, font: { size: 11 } },
            grid: { color: CHART_COLORS.grid },
        },
        y: {
            ticks: { color: CHART_COLORS.text, font: { size: 11 } },
            grid: { color: CHART_COLORS.grid },
            beginAtZero: true,
        },
    },
};

/**
 * Create a real-time line chart for streaming angle data.
 */
function createRealtimeChart(canvasId, options = {}) {
    const ctx = document.getElementById(canvasId);
    if (!ctx) return null;

    const maxPoints = options.maxPoints || 60;
    const datasets = options.datasets || [
        { label: 'Left', borderColor: CHART_COLORS.left, backgroundColor: CHART_COLORS.leftFill },
        { label: 'Right', borderColor: CHART_COLORS.right, backgroundColor: CHART_COLORS.rightFill },
    ];

    const chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: datasets.map(ds => ({
                label: ds.label,
                data: [],
                borderColor: ds.borderColor,
                backgroundColor: ds.backgroundColor,
                borderWidth: 2,
                pointRadius: 0,
                fill: true,
                tension: 0.3,
            })),
        },
        options: {
            ...CHART_DEFAULTS,
            animation: { duration: 0 },
            plugins: {
                ...CHART_DEFAULTS.plugins,
                title: options.title ? {
                    display: true,
                    text: options.title,
                    color: CHART_COLORS.text,
                    font: { size: 14 },
                } : undefined,
            },
            scales: {
                ...CHART_DEFAULTS.scales,
                y: {
                    ...CHART_DEFAULTS.scales.y,
                    min: options.min ?? 0,
                    max: options.max ?? 180,
                    title: {
                        display: true,
                        text: options.yLabel || 'Angle (°)',
                        color: CHART_COLORS.text,
                    },
                },
            },
        },
    });

    chart._maxPoints = maxPoints;
    return chart;
}

/**
 * Push a new data point to a real-time chart.
 */
function pushChartData(chart, label, values) {
    if (!chart) return;
    chart.data.labels.push(label);
    values.forEach((val, i) => {
        chart.data.datasets[i].data.push(val);
    });

    const max = chart._maxPoints || 60;
    while (chart.data.labels.length > max) {
        chart.data.labels.shift();
        chart.data.datasets.forEach(ds => ds.data.shift());
    }

    chart.update('none');
}

/**
 * Create a horizontal bar chart for ROM comparison.
 */
function createComparisonChart(canvasId, movements, leftData, rightData) {
    const ctx = document.getElementById(canvasId);
    if (!ctx) return null;

    return new Chart(ctx, {
        type: 'bar',
        data: {
            labels: movements.map(m => formatMovementName(m)),
            datasets: [
                {
                    label: 'Left Shoulder',
                    data: leftData,
                    backgroundColor: CHART_COLORS.left,
                    borderColor: CHART_COLORS.left,
                    borderWidth: 1,
                },
                {
                    label: 'Right Shoulder',
                    data: rightData,
                    backgroundColor: CHART_COLORS.right,
                    borderColor: CHART_COLORS.right,
                    borderWidth: 1,
                },
            ],
        },
        options: {
            ...CHART_DEFAULTS,
            indexAxis: 'y',
            plugins: {
                ...CHART_DEFAULTS.plugins,
                title: {
                    display: true,
                    text: 'Left vs Right ROM Comparison',
                    color: CHART_COLORS.text,
                    font: { size: 14 },
                },
            },
            scales: {
                x: {
                    ...CHART_DEFAULTS.scales.x,
                    title: {
                        display: true,
                        text: 'ROM (°)',
                        color: CHART_COLORS.text,
                    },
                },
                y: {
                    ...CHART_DEFAULTS.scales.y,
                    beginAtZero: true,
                },
            },
        },
    });
}

/**
 * Create a radar chart for overall ROM profile.
 */
function createRadarChart(canvasId, movements, leftData, rightData) {
    const ctx = document.getElementById(canvasId);
    if (!ctx) return null;

    return new Chart(ctx, {
        type: 'radar',
        data: {
            labels: movements.map(m => formatMovementName(m)),
            datasets: [
                {
                    label: 'Left',
                    data: leftData,
                    borderColor: CHART_COLORS.left,
                    backgroundColor: CHART_COLORS.leftFill,
                    pointBackgroundColor: CHART_COLORS.left,
                    borderWidth: 2,
                },
                {
                    label: 'Right',
                    data: rightData,
                    borderColor: CHART_COLORS.right,
                    backgroundColor: CHART_COLORS.rightFill,
                    pointBackgroundColor: CHART_COLORS.right,
                    borderWidth: 2,
                },
            ],
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: {
                    labels: { color: CHART_COLORS.text, font: { size: 12 } },
                },
            },
            scales: {
                r: {
                    ticks: { color: CHART_COLORS.text, backdropColor: 'transparent' },
                    grid: { color: CHART_COLORS.grid },
                    angleLines: { color: CHART_COLORS.grid },
                    pointLabels: { color: CHART_COLORS.text, font: { size: 11 } },
                    beginAtZero: true,
                },
            },
        },
    });
}

/**
 * Update an existing chart's data in place.
 */
function updateChart(chart, labels, datasets) {
    if (!chart) return;
    chart.data.labels = labels;
    datasets.forEach((ds, i) => {
        if (chart.data.datasets[i]) {
            chart.data.datasets[i].data = ds.data;
            if (ds.label) chart.data.datasets[i].label = ds.label;
        }
    });
    chart.update();
}

/**
 * Format movement snake_case to Title Case.
 */
function formatMovementName(name) {
    return name
        .split('_')
        .map(w => w.charAt(0).toUpperCase() + w.slice(1))
        .join(' ');
}

window.ChartUtils = {
    createRealtimeChart,
    pushChartData,
    createComparisonChart,
    createRadarChart,
    updateChart,
    formatMovementName,
    CHART_COLORS,
};
/// =======================================
/// DyeWarsServer - Debug HTTP Server
/// =======================================
#include "DebugHttpServer.h"
#include "core/Log.h"
#include <sstream>

DebugHttpServer::DebugHttpServer(asio::io_context& io_context, uint16_t port)
    : io_context_(io_context),
      acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)),
      port_(port) {
}

DebugHttpServer::~DebugHttpServer() {
    Stop();
}

void DebugHttpServer::SetStatsProvider(StatsProvider provider) {
    stats_provider_ = std::move(provider);
}

void DebugHttpServer::Start() {
    if (running_.exchange(true)) return;
    Log::Info("Debug HTTP server starting on port {}...", port_);
    StartAccept();
}

void DebugHttpServer::Stop() {
    if (!running_.exchange(false)) return;
    std::error_code ec;
    acceptor_.close(ec);
}

void DebugHttpServer::StartAccept() {
    acceptor_.async_accept([this](const std::error_code ec, asio::ip::tcp::socket socket) {
        if (!ec && running_) {
            HandleConnection(std::move(socket));
        }
        if (running_) {
            StartAccept();
        }
    });
}

void DebugHttpServer::HandleConnection(asio::ip::tcp::socket socket) {
    // Read HTTP request (simple blocking read for debug server)
    auto self = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
    auto buffer = std::make_shared<std::array<char, 1024>>();

    self->async_read_some(asio::buffer(*buffer),
        [this, self, buffer](const std::error_code ec, size_t bytes) {
            if (ec) return;

            std::string request(buffer->data(), bytes);

            // Parse path from "GET /path HTTP/1.1"
            std::string path = "/";
            if (request.substr(0, 4) == "GET ") {
                size_t end = request.find(' ', 4);
                if (end != std::string::npos) {
                    path = request.substr(4, end - 4);
                }
            }

            // Build and send response
            std::string response = BuildResponse(path);
            auto response_buf = std::make_shared<std::string>(std::move(response));

            asio::async_write(*self, asio::buffer(*response_buf),
                [self, response_buf](const std::error_code, size_t) {
                    // Connection closes when self goes out of scope
                });
        });
}

std::string DebugHttpServer::BuildResponse(const std::string& path) {
    std::string body;
    std::string content_type = "text/html";

    if (path == "/stats" || path == "/stats.json") {
        body = GetStatsJson();
        content_type = "application/json";
    } else {
        body = GetDashboardHtml();
    }

    std::ostringstream response;
    response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: " << content_type << "; charset=utf-8\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << body;

    return response.str();
}

std::string DebugHttpServer::GetStatsJson() {
    if (stats_provider_) {
        return stats_provider_();
    }
    return R"({"error": "No stats provider configured"})";
}

std::string DebugHttpServer::GetDashboardHtml() {
    return R"html(<!DOCTYPE html>
<html>
<head>
    <title>DyeWars Server Debug</title>
    <meta charset="utf-8">
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: 'Segoe UI', Consolas, monospace;
            background: #1a1a2e;
            color: #eee;
            padding: 20px;
        }
        h1 { color: #00d4ff; margin-bottom: 20px; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .card {
            background: #16213e;
            border-radius: 10px;
            padding: 20px;
            border: 1px solid #0f3460;
        }
        .card h2 { color: #00d4ff; font-size: 14px; margin-bottom: 15px; text-transform: uppercase; }
        .stat { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #0f3460; }
        .stat:last-child { border-bottom: none; }
        .stat-label { color: #888; }
        .stat-value { color: #00ff88; font-weight: bold; }
        .stat-value.warning { color: #ffaa00; }
        .stat-value.danger { color: #ff4444; }
        .chart { height: 100px; display: flex; align-items: flex-end; gap: 2px; margin-top: 10px; }
        .bar { background: #00d4ff; flex: 1; min-width: 4px; transition: height 0.2s; }
        .status { display: inline-block; width: 10px; height: 10px; border-radius: 50%; margin-right: 8px; }
        .status.online { background: #00ff88; }
        .status.offline { background: #ff4444; }
        #refresh-indicator { position: fixed; top: 10px; right: 10px; color: #666; font-size: 12px; }
    </style>
</head>
<body>
    <h1><span class="status online" id="status"></span>DyeWars Server Debug</h1>
    <div id="refresh-indicator">Refreshing...</div>

    <div class="grid">
        <div class="card">
            <h2>Performance</h2>
            <div class="stat">
                <span class="stat-label">Tick Time (avg)</span>
                <span class="stat-value" id="tick-avg">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Tick Time (max)</span>
                <span class="stat-value" id="tick-max">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">TPS</span>
                <span class="stat-value" id="tps">-</span>
            </div>
            <div class="chart" id="tick-chart"></div>
        </div>

        <div class="card">
            <h2>Connections</h2>
            <div class="stat">
                <span class="stat-label">Real Clients</span>
                <span class="stat-value" id="real-clients">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Fake Clients (Bots)</span>
                <span class="stat-value" id="fake-clients">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Total Players</span>
                <span class="stat-value" id="total-players">-</span>
            </div>
        </div>

        <div class="card">
            <h2>World</h2>
            <div class="stat">
                <span class="stat-label">Visibility Tracked</span>
                <span class="stat-value" id="visibility">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Dirty Players/Tick</span>
                <span class="stat-value" id="dirty-players">-</span>
            </div>
        </div>

        <div class="card">
            <h2>Bandwidth (Out)</h2>
            <div class="stat">
                <span class="stat-label">Current</span>
                <span class="stat-value" id="bytes-out">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Average</span>
                <span class="stat-value" id="bytes-out-avg">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Total</span>
                <span class="stat-value" id="bytes-out-total">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Packets/sec</span>
                <span class="stat-value" id="packets-out">-</span>
            </div>
        </div>

        <div class="card">
            <h2>Bot Movement</h2>
            <div class="stat">
                <span class="stat-label">Spatial Query Time</span>
                <span class="stat-value" id="spatial-time">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Visibility Time</span>
                <span class="stat-value" id="visibility-time">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Departure Time</span>
                <span class="stat-value" id="departure-time">-</span>
            </div>
        </div>

        <div class="card">
            <h2>Broadcast Breakdown</h2>
            <div class="stat">
                <span class="stat-label">Total Time</span>
                <span class="stat-value" id="broadcast-time">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Viewer Query</span>
                <span class="stat-value" id="broadcast-viewer">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Client Lookup</span>
                <span class="stat-value" id="broadcast-lookup">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Packet Send</span>
                <span class="stat-value" id="broadcast-send">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Viewer Count</span>
                <span class="stat-value" id="broadcast-viewers">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Dirty Count</span>
                <span class="stat-value" id="broadcast-dirty">-</span>
            </div>
        </div>

        <div class="card">
            <h2>Viewer Query Detail</h2>
            <div class="stat">
                <span class="stat-label">Spatial Hash</span>
                <span class="stat-value" id="vq-spatial">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">AddKnown()</span>
                <span class="stat-value" id="vq-addknown">-</span>
            </div>
            <div class="stat">
                <span class="stat-label">Total Nearby</span>
                <span class="stat-value" id="vq-nearby">-</span>
            </div>
        </div>
    </div>

    <script>
        const tickHistory = [];
        const maxHistory = 60;

        function formatBytes(bytes) {
            if (bytes < 1024) return bytes + ' B';
            if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB';
            return (bytes / 1024 / 1024).toFixed(2) + ' MB';
        }

        function formatMs(ms) {
            return ms.toFixed(2) + ' ms';
        }

        function updateChart() {
            const chart = document.getElementById('tick-chart');
            const maxVal = Math.max(50, ...tickHistory);
            chart.innerHTML = tickHistory.map(v =>
                `<div class="bar" style="height: ${(v / maxVal) * 100}%; background: ${v > 50 ? '#ff4444' : v > 40 ? '#ffaa00' : '#00d4ff'}"></div>`
            ).join('');
        }

        function setValueWithClass(id, value, thresholds) {
            const el = document.getElementById(id);
            el.textContent = value;
            el.className = 'stat-value';
            if (thresholds) {
                const numVal = parseFloat(value);
                if (numVal >= thresholds.danger) el.classList.add('danger');
                else if (numVal >= thresholds.warning) el.classList.add('warning');
            }
        }

        async function refresh() {
            try {
                const res = await fetch('/stats');
                const data = await res.json();

                document.getElementById('status').className = 'status online';
                document.getElementById('refresh-indicator').textContent = 'Last update: ' + new Date().toLocaleTimeString();

                // Performance
                setValueWithClass('tick-avg', formatMs(data.tick_avg_ms || 0), {warning: 40, danger: 50});
                setValueWithClass('tick-max', formatMs(data.tick_max_ms || 0), {warning: 50, danger: 100});
                document.getElementById('tps').textContent = (data.tps || 0).toFixed(1);

                // Track history
                if (data.tick_avg_ms !== undefined) {
                    tickHistory.push(data.tick_avg_ms);
                    if (tickHistory.length > maxHistory) tickHistory.shift();
                    updateChart();
                }

                // Connections
                document.getElementById('real-clients').textContent = data.real_clients || 0;
                document.getElementById('fake-clients').textContent = data.fake_clients || 0;
                document.getElementById('total-players').textContent = data.total_players || 0;

                // World
                document.getElementById('visibility').textContent = data.visibility_tracked || 0;
                document.getElementById('dirty-players').textContent = data.dirty_players || 0;

                // Bandwidth
                document.getElementById('bytes-out').textContent = formatBytes(data.bytes_out_per_sec || 0) + '/s';
                document.getElementById('bytes-out-avg').textContent = formatBytes(data.bytes_out_avg || 0) + '/s';
                document.getElementById('bytes-out-total').textContent = formatBytes(data.bytes_out_total || 0);
                document.getElementById('packets-out').textContent = data.packets_out_per_sec || 0;

                // Bot movement breakdown
                document.getElementById('spatial-time').textContent = formatMs(data.spatial_time_ms || 0);
                document.getElementById('visibility-time').textContent = formatMs(data.visibility_time_ms || 0);
                document.getElementById('departure-time').textContent = formatMs(data.departure_time_ms || 0);

                // Broadcast breakdown
                setValueWithClass('broadcast-time', formatMs(data.broadcast_time_ms || 0), {warning: 20, danger: 40});
                setValueWithClass('broadcast-viewer', formatMs(data.broadcast_viewer_ms || 0), {warning: 10, danger: 20});
                setValueWithClass('broadcast-lookup', formatMs(data.broadcast_lookup_ms || 0), {warning: 5, danger: 10});
                setValueWithClass('broadcast-send', formatMs(data.broadcast_send_ms || 0), {warning: 10, danger: 20});
                document.getElementById('broadcast-viewers').textContent = data.broadcast_viewer_count || 0;
                document.getElementById('broadcast-dirty').textContent = data.broadcast_dirty_count || 0;

                // Viewer query sub-breakdown
                setValueWithClass('vq-spatial', formatMs(data.vq_spatial_ms || 0), {warning: 5, danger: 10});
                setValueWithClass('vq-addknown', formatMs(data.vq_addknown_ms || 0), {warning: 10, danger: 20});
                document.getElementById('vq-nearby').textContent = data.vq_nearby_count || 0;

            } catch (e) {
                document.getElementById('status').className = 'status offline';
                document.getElementById('refresh-indicator').textContent = 'Connection lost';
            }
        }

        // Refresh every 500ms
        refresh();
        setInterval(refresh, 500);
    </script>
</body>
</html>
)html";
}
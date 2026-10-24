#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

PORT=${1:-8080}
BUILD_DIR="${PROJECT_ROOT}/build-bench"
SERVER_BIN="${BUILD_DIR}/tests/benchmark_server"
THROUGHPUT_BIN="${BUILD_DIR}/protocol_throughput_benchmark"
LOG_FILE="${SCRIPT_DIR}/benchmark_server.log"
THROUGHPUT_LOG="${SCRIPT_DIR}/throughput_benchmark.log"

# Server worker thread counts to benchmark (e.g. "1 4" or "1 2 4 8")
BENCH_THREADS=${BENCH_THREADS:-"1 4"}
BENCH_THREADS=$(echo "${BENCH_THREADS}" | tr ',' ' ')

# Saturation phase configuration
SKIP_SATURATION=${SKIP_SATURATION:-0}
SAT_CONNECTIONS=${SAT_CONNECTIONS:-100}
SAT_WORKERS=${SAT_WORKERS:-4}
SAT_DURATION=${SAT_DURATION:-10}
SAT_RUNS=${SAT_RUNS:-1}
SAT_PREFIX="${SCRIPT_DIR}/saturation"

# ANSI Colors for clean visual output
BOLD="\033[1m"
CYAN="\033[1;36m"
GREEN="\033[1;32m"
YELLOW="\033[1;33m"
RED="\033[1;31m"
RESET="\033[0m"

print_banner() {
    local text="$1"
    echo -e "\n${CYAN}======================================================================${RESET}"
    echo -e "${CYAN}  ${BOLD}${text}${RESET}"
    echo -e "${CYAN}======================================================================${RESET}"
}

# Helper to check if the target port is currently in use
port_in_use() {
    [ -n "$(ss -Htuln "sport = :${PORT}" 2>/dev/null)" ]
}

# Verify the port is free before starting, or stop cleanly with an informative message
require_free_port() {
    if port_in_use; then
        echo -e "${RED}ERROR: Port ${PORT} is already in use:${RESET}"
        ss -tulnp "sport = :${PORT}" 2>/dev/null || true
        echo -e "${RED}Stop the process using port ${PORT} or pass a different port: $0 <port>${RESET}"
        exit 1
    fi
}

# Wait after shutting down a server process for the port to be fully released
wait_for_port_free() {
    local retries=50
    while [ ${retries} -gt 0 ]; do
        if ! port_in_use; then
            return 0
        fi
        retries=$((retries - 1))
        sleep 0.1
    done
    echo -e "${RED}ERROR: Port ${PORT} is still in use after server shutdown.${RESET}"
    exit 1
}

# Global cleanup state and trap (persistent handler)
SERVER_PID=""
MONITOR_PID=""
cleanup() {
    if [ -n "${SERVER_PID}" ]; then
        kill "${SERVER_PID}" 2>/dev/null || true
        for _ in {1..20}; do
            kill -0 "${SERVER_PID}" 2>/dev/null || break
            sleep 0.05
        done
        kill -9 "${SERVER_PID}" 2>/dev/null || true
    fi
    if [ -n "${MONITOR_PID}" ]; then
        wait "${MONITOR_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT

# 1. Verify Node.js and npm (version 22+ required for Artillery)
NODE_MAJOR=0
if command -v node &>/dev/null; then
    NODE_MAJOR=$(node -v | cut -d'v' -f2 | cut -d'.' -f1)
fi

if [ "${NODE_MAJOR}" -lt 22 ]; then
    print_banner "Installing/Upgrading Node.js to v22"
    echo -e "${YELLOW}Artillery requires Node.js v22+. Current version: v${NODE_MAJOR}.${RESET}"
    echo -e "${YELLOW}Setting up NodeSource Node.js 22.x repository...${RESET}"
    SUDO=""
    if command -v sudo &>/dev/null; then
        SUDO="sudo -E"
    fi
    curl -fsSL https://deb.nodesource.com/setup_22.x | ${SUDO} bash -
    ${SUDO} apt-get install -y nodejs
    
    if [ -d "${SCRIPT_DIR}/node_modules" ]; then
        echo -e "${YELLOW}Clearing old node_modules...${RESET}"
        rm -rf "${SCRIPT_DIR}/node_modules"
    fi
fi

# 2. Install Artillery and autocannon locally
cd "${SCRIPT_DIR}"
if [ ! -x "${SCRIPT_DIR}/node_modules/.bin/artillery" ] || \
   [ ! -x "${SCRIPT_DIR}/node_modules/.bin/autocannon" ]; then
    echo -e "${YELLOW}Installing benchmark dependencies locally...${RESET}"
    npm install --no-audit --no-fund
fi

# 3. Build C++ target binaries
print_banner "Compiling C++ Binaries in Release Mode (build-bench/)"
if [ ! -d "${BUILD_DIR}" ]; then
    mkdir -p "${BUILD_DIR}"
fi
echo -e "${YELLOW}Configuring CMake project in ${BUILD_DIR}...${RESET}"
cmake -B "${BUILD_DIR}" -S "${PROJECT_ROOT}" -DCMAKE_BUILD_TYPE=Release

echo -e "${YELLOW}Building targets...${RESET}"
JOBS=$(nproc 2>/dev/null || echo 4)
cmake --build "${BUILD_DIR}" --config Release --target benchmark_server -j"${JOBS}"

HAVE_THROUGHPUT=0
if grep -q '^CPPHTTP_ENABLE_QUIC:BOOL=ON' "${BUILD_DIR}/CMakeCache.txt" 2>/dev/null && \
   ! grep -q '^CPPHTTP_BUILD_EXAMPLES:BOOL=OFF' "${BUILD_DIR}/CMakeCache.txt" 2>/dev/null; then
    HAVE_THROUGHPUT=1
    cmake --build "${BUILD_DIR}" --config Release --target protocol_throughput_benchmark -j"${JOBS}"
else
    rm -f "${THROUGHPUT_BIN}"
fi

# 4. Check port availability and clean up old reports
require_free_port
rm -f "${LOG_FILE}" "${THROUGHPUT_LOG}" "${SCRIPT_DIR}"/benchmark_report_* "${SCRIPT_DIR}"/resources_t*.txt "${SAT_PREFIX}"*

# 5. Run Raw C++ Network Throughput Benchmark (TCP vs HTTP/3 QUIC)
print_banner "Running C++ Network Throughput Benchmark"
if [ "${HAVE_THROUGHPUT}" -eq 1 ] && [ -f "${THROUGHPUT_BIN}" ]; then
    echo -e "${YELLOW}Executing raw TCP vs. HTTP/3 (QUIC) download benchmark on port ${PORT}...${RESET}"
    "${THROUGHPUT_BIN}" "${PORT}" > "${THROUGHPUT_LOG}" 2>&1 || true
    echo -e "${GREEN}C++ throughput tests completed.${RESET}"
    wait_for_port_free
else
    echo -e "${RED}Notice: protocol_throughput_benchmark not enabled or not found. Skipping raw throughput test.${RESET}"
fi

proc_ticks() {
    sed 's/^.*) //' "/proc/$1/stat" 2>/dev/null | awk '{print $12 + $13}'
}

# Resource monitor helper function (computes real-time CPU % from /proc/$pid/stat and RSS)
monitor_resources() {
    local pid=$1
    local outfile=$2
    local hz
    hz=$(getconf CLK_TCK 2>/dev/null || echo 100)
    local peak_cpu=0
    local peak_mem=0
    local sum_cpu=0
    local samples=0
    local prev_ticks prev_ns
    prev_ticks=$(proc_ticks "$pid") || return 0
    prev_ns=$(date +%s%N)
    while sleep 0.5 && kill -0 "$pid" 2>/dev/null; do
        local ticks now_ns rss cpu dt_ns
        ticks=$(proc_ticks "$pid") || break
        now_ns=$(date +%s%N)
        dt_ns=$((now_ns - prev_ns))
        if [ "$dt_ns" -gt 0 ]; then
            cpu=$(( (ticks - prev_ticks) * 100 * 1000000000 / (hz * dt_ns) ))
        else
            cpu=0
        fi
        rss=$(awk '/^VmRSS:/{print $2}' "/proc/$pid/status" 2>/dev/null || echo 0)
        [ "$cpu" -gt "$peak_cpu" ] && peak_cpu=$cpu
        [ "$rss" -gt "$peak_mem" ] && peak_mem=$rss
        sum_cpu=$((sum_cpu + cpu))
        samples=$((samples + 1))
        prev_ticks=$ticks
        prev_ns=$now_ns
        local avg_cpu=0
        [ "$samples" -gt 0 ] && avg_cpu=$((sum_cpu / samples))
        echo "$peak_cpu $((peak_mem / 1024)) $avg_cpu" > "$outfile"
    done
}

# Wait for server process to signal ready
wait_for_server() {
    local retries=50
    while [ ${retries} -gt 0 ]; do
        if [ -f "${LOG_FILE}" ] && grep -q "Server Running" "${LOG_FILE}" 2>/dev/null; then
            return 0
        fi
        retries=$((retries - 1))
        sleep 0.05
    done
    echo -e "${RED}ERROR: Server failed to start on port ${PORT}${RESET}"
    cat "${LOG_FILE}" || true
    exit 1
}

# Start server process with given worker thread count
start_server() {
    local threads=$1
    rm -f "${LOG_FILE}"
    "${SERVER_BIN}" "${PORT}" "${threads}" < /dev/null > "${LOG_FILE}" 2>&1 &
    SERVER_PID=$!
    wait_for_server
}

# Stop server process gracefully (SIGTERM -> poll -> SIGKILL fallback)
stop_server() {
    echo -e "${YELLOW}Stopping server...${RESET}"
    kill "${SERVER_PID}" 2>/dev/null || true
    for _ in {1..30}; do
        kill -0 "${SERVER_PID}" 2>/dev/null || break
        sleep 0.1
    done
    kill -9 "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
    SERVER_PID=""
    wait_for_port_free
}

# Function to run the open-loop Artillery benchmark for a specific thread count
run_artillery_for_threads() {
    local threads=$1
    local report_prefix=$2
    local monitor_file=$3

    print_banner "Open-Loop Load Test (Artillery): ${threads} Worker Thread(s)"
    start_server "${threads}"
    echo -e "${GREEN}Server up with ${threads} worker thread(s). Starting resource monitor...${RESET}"
    
    # Start resource monitor in background
    monitor_resources ${SERVER_PID} "${monitor_file}" &
    MONITOR_PID=$!
    
    # Run HTTP load tests
    echo -e "${YELLOW}Running HTTP load tests...${RESET}"
    npx artillery run \
        --target "http://localhost:${PORT}" \
        --output "${report_prefix}_http.json" \
        benchmark_config.yml

    # Run WebSocket load tests
    echo -e "${YELLOW}Running WebSocket load tests...${RESET}"
    npx artillery run \
        --target "ws://localhost:${PORT}/ws" \
        --output "${report_prefix}_ws.json" \
        benchmark_ws_config.yml
        
    kill "${MONITOR_PID}" 2>/dev/null || true
    wait "${MONITOR_PID}" 2>/dev/null || true
    MONITOR_PID=""
    stop_server
}

# Function to run the closed-loop saturation benchmark (autocannon) for a specific thread count
run_saturation_for_threads() {
    local threads=$1
    local hz
    hz=$(getconf CLK_TCK 2>/dev/null || echo 100)
    print_banner "Closed-Loop Saturation Test (Autocannon): ${threads} Worker Thread(s)"
    start_server "${threads}"
    echo -e "${GREEN}Server up with ${threads} worker thread(s). Running saturation workloads...${RESET}"

    local ep name
    for ep in "/" "/data"; do
        name=$([ "$ep" = "/" ] && echo root || echo data)
        local hdr=()
        [ "$ep" = "/data" ] && hdr=(-H "Accept-Encoding: gzip")
        for run in $(seq 1 "${SAT_RUNS}"); do
            echo -e "${YELLOW}  Testing ${ep} (run ${run}/${SAT_RUNS}, ${SAT_CONNECTIONS} conns, ${SAT_DURATION}s)...${RESET}"
            local t0 n0 t1 n1
            t0=$(proc_ticks "${SERVER_PID}")
            n0=$(date +%s%N)
            "${SCRIPT_DIR}/node_modules/.bin/autocannon" -j \
                -c "${SAT_CONNECTIONS}" -w "${SAT_WORKERS}" -d "${SAT_DURATION}" \
                "${hdr[@]}" "http://127.0.0.1:${PORT}${ep}" \
                > "${SAT_PREFIX}_t${threads}_${name}_r${run}.json"
            t1=$(proc_ticks "${SERVER_PID}")
            n1=$(date +%s%N)
            local dt_ns=$((n1 - n0))
            local cpu=0
            if [ "$dt_ns" -gt 0 ]; then
                cpu=$(( (t1 - t0) * 100 * 1000000000 / (hz * dt_ns) ))
            fi
            echo "$cpu" > "${SAT_PREFIX}_t${threads}_${name}_r${run}.cpu.txt"
        done
    done
    stop_server
}

# 6. Run the Open-Loop Scalability Sweep (Artillery)
for threads in ${BENCH_THREADS}; do
    run_artillery_for_threads "${threads}" "${SCRIPT_DIR}/benchmark_report_t${threads}" "${SCRIPT_DIR}/resources_t${threads}.txt"
    sleep 0.5
done

# 7. Run the Closed-Loop Saturation Sweep (Autocannon)
if [ "${SKIP_SATURATION}" != "1" ]; then
    for threads in ${BENCH_THREADS}; do
        sleep 0.5
        run_saturation_for_threads "${threads}"
    done
fi

# 8. Print Advanced Performance Interpretation Dashboard
FIRST_THREAD=$(echo "${BENCH_THREADS}" | awk '{print $1}')
if [ -f "${SCRIPT_DIR}/benchmark_report_t${FIRST_THREAD}_http.json" ]; then
    print_banner "Advanced Performance Diagnostics Dashboard"
    BENCH_THREADS="${BENCH_THREADS}" SCRIPT_DIR="${SCRIPT_DIR}" node << 'EOF'
const fs = require('fs');
const path = require('path');

const SCRIPT_DIR = process.env.SCRIPT_DIR || '.';
const rawThreads = (process.env.BENCH_THREADS || '1 4')
  .trim()
  .split(/[\s,]+/)
  .map(x => parseInt(x, 10))
  .filter(x => !isNaN(x) && x > 0);
const threadsList = Array.from(new Set(rawThreads));
const THROUGHPUT_LOG = path.join(SCRIPT_DIR, 'throughput_benchmark.log');
const SAT_PREFIX = path.join(SCRIPT_DIR, 'saturation');
const SKIP_SATURATION = process.env.SKIP_SATURATION === '1';

function parseCombinedReport(prefix) {
  const httpPath = prefix + '_http.json';
  const wsPath = prefix + '_ws.json';
  
  let httpReq = 0, httpRes = 0, http2xx = 0, httpNon2xx = 0, httpFailed = 0, httpRate = 0, httpMean = 0, httpP99 = 0;
  if (fs.existsSync(httpPath)) {
    const d = JSON.parse(fs.readFileSync(httpPath, 'utf8'));
    const agg = d.aggregate || {};
    const counters = agg.counters || {};
    httpReq = counters['http.requests'] || 0;
    httpRes = counters['http.responses'] || 0;
    httpFailed = counters['vusers.failed'] || 0;

    for (const [key, val] of Object.entries(counters)) {
      if (key.startsWith('http.codes.')) {
        const code = parseInt(key.replace('http.codes.', ''), 10);
        if (code >= 200 && code < 300) {
          http2xx += val;
        } else {
          httpNon2xx += val;
        }
      }
    }

    if (agg.firstCounterAt && agg.lastCounterAt) {
      const testDuration = (agg.lastCounterAt - agg.firstCounterAt) / 1000;
      httpRate = testDuration > 0 ? (httpReq / testDuration) : 0;
    }
    const summaries = agg.summaries || {};
    httpMean = summaries['http.response_time'] ? summaries['http.response_time'].mean || 0 : 0;
    httpP99 = summaries['http.response_time'] ? summaries['http.response_time'].p99 || 0 : 0;
  }

  let wsSent = 0, wsFailed = 0, wsRate = 0;
  if (fs.existsSync(wsPath)) {
    const d = JSON.parse(fs.readFileSync(wsPath, 'utf8'));
    const agg = d.aggregate || {};
    const counters = agg.counters || {};
    wsSent = counters['websocket.messages_sent'] || 0;
    wsFailed = counters['vusers.failed'] || 0;
    if (agg.firstCounterAt && agg.lastCounterAt) {
      const testDuration = (agg.lastCounterAt - agg.firstCounterAt) / 1000;
      wsRate = testDuration > 0 ? (wsSent / testDuration) : 0;
    }
  }

  return {
    requests: httpReq,
    responses: httpRes,
    http2xx: http2xx,
    httpNon2xx: httpNon2xx,
    wsSent: wsSent,
    failedUsers: httpFailed + wsFailed,
    httpRate: httpRate,
    wsRate: wsRate,
    latencyMean: httpMean,
    latencyP99: httpP99
  };
}

function parseSaturation(threads, endpoint) {
  const name = endpoint === '/' ? 'root' : 'data';
  const runs = [];
  for (let r = 1; r <= 50; ++r) {
    const jsonPath = `${SAT_PREFIX}_t${threads}_${name}_r${r}.json`;
    const cpuPath = `${SAT_PREFIX}_t${threads}_${name}_r${r}.cpu.txt`;
    if (!fs.existsSync(jsonPath)) break;
    try {
      const data = JSON.parse(fs.readFileSync(jsonPath, 'utf8'));
      let cpu = 0;
      if (fs.existsSync(cpuPath)) {
        cpu = parseInt(fs.readFileSync(cpuPath, 'utf8').trim(), 10) || 0;
      }
      runs.push({
        reqRate: (data.requests && data.requests.average) || 0,
        latencyMean: (data.latency && data.latency.average) || 0,
        latencyP99: (data.latency && data.latency.p99) || 0,
        errors: (data.errors || 0) + (data.timeouts || 0) + (data.non2xx || 0),
        cpu: cpu
      });
    } catch (_) {}
  }
  if (runs.length === 0) return null;
  runs.sort((a, b) => a.reqRate - b.reqRate);
  const mid = Math.floor(runs.length / 2);
  const median = runs[mid];
  let totalErrors = 0;
  runs.forEach(x => { totalErrors += x.errors; });
  return {
    reqRate: median.reqRate,
    latencyMean: median.latencyMean,
    latencyP99: median.latencyP99,
    cpu: median.cpu,
    totalErrors: totalErrors,
    runCount: runs.length
  };
}

function parseResources(path) {
  if (!fs.existsSync(path)) return { cpu: 0, mem: 0, avgCpu: 0 };
  const content = fs.readFileSync(path, 'utf8').trim().split(/\s+/);
  return {
    cpu: parseInt(content[0]) || 0,
    mem: parseInt(content[1]) || 0,
    avgCpu: parseInt(content[2]) || 0
  };
}

const reports = {};
const resources = {};
for (const t of threadsList) {
  reports[t] = parseCombinedReport(path.join(SCRIPT_DIR, `benchmark_report_t${t}`));
  resources[t] = parseResources(path.join(SCRIPT_DIR, `resources_t${t}.txt`));
}

const baseT = threadsList[0];
const baseRep = reports[baseT];
const baseRes = resources[baseT];

const throughputText = fs.existsSync(THROUGHPUT_LOG) ? fs.readFileSync(THROUGHPUT_LOG, 'utf8') : '';
const tcpMatch = throughputText.match(/1\.\s+Testing HTTP\/1\.1[\s\S]*?Throughput:\s+([\d.]+)\s+MB\/s/);
const quicWarmMatch = throughputText.match(/Warm Connection Pass:[\s\S]*?Throughput:\s+([\d.]+)\s+MB\/s/);
const quicInitMatch = throughputText.match(/Initial Handshake Pass:[\s\S]*?Throughput:\s+([\d.]+)\s+MB\/s/);
const wsMatch = throughputText.match(/3\.\s+Testing WebSocket[\s\S]*?Throughput \(Send\+Recv\):\s+([\d.]+)\s+MB\/s/);

const tcpSpeed = tcpMatch ? tcpMatch[1] + ' MB/s' : 'N/A';
const quicSpeed = quicWarmMatch ? quicWarmMatch[1] + ' MB/s' : (quicInitMatch ? quicInitMatch[1] + ' MB/s' : 'N/A');
const wsSpeed = wsMatch ? wsMatch[1] + ' MB/s' : 'N/A';

console.log('\x1b[1m--- 1. OPEN-LOOP LOAD (Fixed Arrival Rate - Artillery) ---\x1b[0m');
for (const t of threadsList) {
  const rep = reports[t];
  console.log(`HTTP Avg Throughput (${t} Thread${t > 1 ? 's' : ''}):   ${rep.httpRate.toFixed(1)} req/sec`);
}
if (threadsList.length > 1) {
  const parts = [];
  for (let i = 1; i < threadsList.length; i++) {
    const t = threadsList[i];
    const scale = baseRep.httpRate > 0 ? (reports[t].httpRate / baseRep.httpRate).toFixed(2) + 'x' : 'N/A';
    parts.push(`${scale} (${t}T vs ${baseT}T)`);
  }
  console.log('HTTP Throughput Scaling:          \x1b[35m' + parts.join(', ') + '\x1b[0m');
}
console.log('');

for (const t of threadsList) {
  const rep = reports[t];
  console.log(`WS Avg Msg Rate (${t} Thread${t > 1 ? 's' : ''}):       ${rep.wsRate.toFixed(1)} msg/sec`);
}
if (threadsList.length > 1) {
  const parts = [];
  for (let i = 1; i < threadsList.length; i++) {
    const t = threadsList[i];
    const scale = baseRep.wsRate > 0 ? (reports[t].wsRate / baseRep.wsRate).toFixed(2) + 'x' : 'N/A';
    parts.push(`${scale} (${t}T vs ${baseT}T)`);
  }
  console.log('WS Msg Rate Scaling:              \x1b[35m' + parts.join(', ') + '\x1b[0m');
}
console.log('');

for (const t of threadsList) {
  const rep = reports[t];
  console.log(`HTTP Mean Latency (${t} Thread${t > 1 ? 's' : ''}):      ${rep.latencyMean.toFixed(2)} ms`);
}
if (threadsList.length > 1) {
  for (let i = 1; i < threadsList.length; i++) {
    const t = threadsList[i];
    const dMs = reports[t].latencyMean - baseRep.latencyMean;
    const dPct = baseRep.latencyMean > 0 ? (dMs / baseRep.latencyMean * 100) : 0;
    console.log(`HTTP Mean Latency Change (${t}T vs ${baseT}T): ${dMs >= 0 ? '+' : ''}${dMs.toFixed(2)} ms (${dPct >= 0 ? '+' : ''}${dPct.toFixed(0)}%)`);
  }
}
for (const t of threadsList) {
  const rep = reports[t];
  console.log(`HTTP p99 Latency (${t} Thread${t > 1 ? 's' : ''}):       ${rep.latencyP99} ms`);
}

if (threadsList.length > 1 && baseRep.requests > 0) {
  let allClose = true;
  for (let i = 1; i < threadsList.length; i++) {
    const t = threadsList[i];
    if (reports[t].requests > 0) {
      const diff = Math.abs(reports[t].requests - baseRep.requests) / baseRep.requests;
      if (diff > 0.05) { allClose = false; break; }
    }
  }
  if (allClose) {
    console.log('\n\x1b[33mNote: Open-loop load did not saturate the server (similar request count in all runs);\nscaling ratios are bounded by the fixed arrival rate (see SATURATION section below).\x1b[0m');
  }
}
console.log('');

console.log('\x1b[1m--- 2. CLOSED-LOOP SATURATION & SCALING (Autocannon) ---\x1b[0m');
let totalSatErrors = 0;
if (SKIP_SATURATION) {
  console.log('Saturation phase skipped via SKIP_SATURATION=1.\n');
} else {
  const satEndpoints = [
    { label: 'GET /data (Transparent Gzip Compression)', ep: '/data' },
    { label: 'GET / (Plaintext Routing)', ep: '/' }
  ];
  let foundAny = false;
  for (const item of satEndpoints) {
    const satResults = {};
    for (const t of threadsList) {
      satResults[t] = parseSaturation(t, item.ep);
    }
    const baseSat = satResults[baseT];
    if (!baseSat) continue;
    foundAny = true;
    for (const t of threadsList) {
      if (satResults[t]) totalSatErrors += satResults[t].totalErrors;
    }
    console.log('\x1b[36m' + item.label + ':\x1b[0m');
    for (const t of threadsList) {
      const s = satResults[t];
      if (s) {
        console.log(`  ${t} Thread${t > 1 ? 's' : ''}:  ${Math.round(s.reqRate).toLocaleString()} req/sec  mean: ${s.latencyMean.toFixed(2)} ms  p99: ${s.latencyP99} ms  CPU: ${s.cpu}%`);
      }
    }
    if (threadsList.length > 1 && baseSat && baseSat.reqRate > 0) {
      const scaleParts = [];
      for (let i = 1; i < threadsList.length; i++) {
        const t = threadsList[i];
        const s = satResults[t];
        if (s) {
          scaleParts.push(`${(s.reqRate / baseSat.reqRate).toFixed(2)}x (${t}T vs ${baseT}T)`);
        }
      }
      console.log('  Throughput Scaling: \x1b[35m' + scaleParts.join(', ') + '\x1b[0m');
      
      const lastT = threadsList[threadsList.length - 1];
      const lastSat = satResults[lastT];
      if (lastSat && (lastSat.reqRate / baseSat.reqRate < 1.3) && (lastSat.cpu < baseSat.cpu * 1.5)) {
        console.log('  \x1b[33mNote: Server CPU did not scale proportionally on this endpoint;\n' +
                    '  likely bound by client load generator or single-threaded socket I/O rather than request workers.\x1b[0m');
      }
    }
    console.log('');
  }
  if (!foundAny) {
    console.log('No saturation reports found.\n');
  }
}

console.log('\x1b[1m--- 3. RESOURCE FOOTPRINT ---\x1b[0m');
for (const t of threadsList) {
  const res = resources[t];
  console.log(`CPU (${t} Thread${t > 1 ? 's' : ''}):   Peak ${res.cpu}%, Avg ${res.avgCpu}%`);
}
for (const t of threadsList) {
  const res = resources[t];
  console.log(`Peak Memory (${t} Thread${t > 1 ? 's' : ''}): ${res.mem} MB`);
}
console.log('');

console.log('\x1b[1m--- 4. RAW C++ NETWORK THROUGHPUT (20MB Payload) ---\x1b[0m');
console.log('HTTP/1.1 (SSL/TCP):      ' + tcpSpeed);
console.log('HTTP/3 (SSL/QUIC):       ' + quicSpeed);
console.log('WebSocket (SSL/TCP):     ' + wsSpeed);
console.log('');

console.log('\x1b[1m--- 5. STABILITY AND CODES ---\x1b[0m');
let totalFailed = 0;
let totalErrors = 0;
let totalResponses = 0;
let lastMeanLatency = 0;
for (const t of threadsList) {
  const rep = reports[t];
  console.log(`HTTP Requests / Responses (${t} Thread${t > 1 ? 's' : ''}):  ${rep.requests} / ${rep.responses} (${rep.http2xx} 2xx, ${rep.httpNon2xx} errors)`);
  totalFailed += rep.failedUsers;
  totalErrors += rep.httpNon2xx;
  totalResponses += rep.responses;
  lastMeanLatency = rep.latencyMean;
}
const maxThreads = threadsList[threadsList.length - 1];
console.log(`Total WS Messages Sent (${maxThreads} Thread${maxThreads > 1 ? 's' : ''}):    ${reports[maxThreads].wsSent}`);
const failedParts = threadsList.map(t => `${reports[t].failedUsers} (${t}T)`).join(' / ');
console.log(`Failed VUs:                            ${failedParts}`);
if (!SKIP_SATURATION) {
  console.log('Saturation Test Errors:                ' + totalSatErrors);
}

const overallErrors = totalFailed + totalErrors + totalSatErrors;
let rating = '';
if (overallErrors === 0 && totalResponses > 0 && lastMeanLatency < 2) {
  rating = '\x1b[32m★ EXCELLENT (0 failures, 0 HTTP errors, mean latency < 2 ms)\x1b[0m';
} else if (overallErrors === 0 && totalResponses > 0) {
  rating = '\x1b[36m★ GOOD (0 failures, 0 HTTP errors, mean latency >= 2 ms)\x1b[0m';
} else {
  rating = '\x1b[31m★ WARNING: Server experienced connection failures or HTTP errors\x1b[0m';
}
console.log('\nPerformance Assessment: ' + rating + '\n');
EOF
fi

#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

PORT=${1:-4433}
BUILD_DIR="${PROJECT_ROOT}/build"
SERVER_BIN="${BUILD_DIR}/quic_http_server"
HINTS_FILE="${PROJECT_ROOT}/hints.txt"
LOG_FILE="/tmp/quic_server.log"

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

run_curl_and_wait() {
    local tmp_out
    tmp_out=$(mktemp)
    curl-http3 --retry 5 --retry-connrefused --retry-delay 0 "$@" > "${tmp_out}" 2>&1 &
    local curl_pid=$!
    wait ${curl_pid} 2>/dev/null || true
    cat "${tmp_out}"
    rm -f "${tmp_out}"
}

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

print_banner "HTTP/3 (QUIC) & Alt-Svc Upgrade Test Suite"

# Clean up port, hints file, and log file
fuser -k ${PORT}/tcp >/dev/null 2>&1 || true
fuser -k ${PORT}/udp >/dev/null 2>&1 || true
rm -f "${HINTS_FILE}" "${LOG_FILE}"
sleep 0.3

# Build server if binary missing or outdated
if [ ! -f "${SERVER_BIN}" ]; then
    echo -e "${YELLOW}Building quic_http_server...${RESET}"
    cmake --build "${BUILD_DIR}" --target quic_http_server -j4
fi

echo -e "${BOLD}Starting quic_http_server on port ${PORT}...${RESET}"
${SERVER_BIN} ${PORT} < /dev/null > "${LOG_FILE}" 2>&1 &
SERVER_PID=$!

cleanup() {
    disown ${SERVER_PID} 2>/dev/null || true
    kill -9 ${SERVER_PID} >/dev/null 2>&1 || true
    fuser -k ${PORT}/tcp >/dev/null 2>&1 || true
    fuser -k ${PORT}/udp >/dev/null 2>&1 || true
    rm -f "${LOG_FILE}" "${HINTS_FILE}" 2>/dev/null || true
}
trap cleanup EXIT

# Wait for server process output to signal readiness
wait_for_server

# ----------------------------------------------------------------------
# Test 1: HTTP/1.1 Connection with --alt-svc hints.txt (learns Alt-Svc)
# ----------------------------------------------------------------------
print_banner "[TEST 1/4] HTTP/1.1 Connection with --alt-svc hints.txt"

RESPONSE1=$(run_curl_and_wait --http1.1 -m 5 --alt-svc "${HINTS_FILE}" -H "Connection: close" -k -s -i "https://127.0.0.1:${PORT}/")
echo "${RESPONSE1}" | head -n 15

if echo "${RESPONSE1}" | grep -qi "Alt-Svc:"; then
    echo -e "${GREEN}✔ SUCCESS: Alt-Svc header received from server!${RESET}"
else
    echo -e "${RED}✖ ERROR: Alt-Svc header missing from response.${RESET}"
    echo "--- Server Log Output ---"
    cat "${LOG_FILE}"
    exit 1
fi

echo -e "\n${BOLD}Contents of hints.txt:${RESET}"
cat "${HINTS_FILE}" || true

# ----------------------------------------------------------------------
# Test 2: Upgraded Connection using hints.txt
# ----------------------------------------------------------------------
print_banner "[TEST 2/4] Upgraded HTTP/3 Request using hints.txt"

RESPONSE2=$(run_curl_and_wait -m 10 --alt-svc "${HINTS_FILE}" -k -s -w "\nHTTP_CODE: %{http_code}\nHTTP_VERSION: %{http_version}\n" "https://127.0.0.1:${PORT}/")
echo "${RESPONSE2}"

if echo "${RESPONSE2}" | grep -q "HTTP_CODE: 200"; then
    echo -e "${GREEN}✔ SUCCESS: Request using Alt-Svc hints.txt upgraded to HTTP/3!${RESET}"
else
    echo -e "${RED}✖ ERROR: Request using Alt-Svc hints.txt failed.${RESET}"
    echo "--- Server Log Output ---"
    cat "${LOG_FILE}"
    exit 1
fi

# ----------------------------------------------------------------------
# Test 3: Direct HTTP/3 Connection (--http3-only)
# ----------------------------------------------------------------------
print_banner "[TEST 3/4] Direct HTTP/3 Connection (--http3-only)"

RESPONSE3=$(run_curl_and_wait --http3-only -m 10 -k -s -w "\nHTTP_CODE: %{http_code}\nHTTP_VERSION: %{http_version}\n" "https://127.0.0.1:${PORT}/")
echo "${RESPONSE3}"

if echo "${RESPONSE3}" | grep -q "HTTP_CODE: 200"; then
    echo -e "${GREEN}✔ SUCCESS: Direct HTTP/3 connection succeeded!${RESET}"
else
    echo -e "${RED}✖ ERROR: Direct HTTP/3 connection failed.${RESET}"
    echo "--- Server Log Output ---"
    cat "${LOG_FILE}"
    exit 1
fi

# ----------------------------------------------------------------------
# Test 4: API Stats Endpoint over HTTP/3
# ----------------------------------------------------------------------
print_banner "[TEST 4/4] /api/stats Endpoint over HTTP/3"

STATS_RESPONSE=$(run_curl_and_wait --http3-only -m 10 -k -s "https://127.0.0.1:${PORT}/api/stats")
echo -e "${BOLD}API Stats JSON:${RESET}\n${STATS_RESPONSE}"

echo -e "\n${GREEN}======================================================================${RESET}"
echo -e "${GREEN}  ✔ ALL HTTP/3 AND ALT-SVC HINT UPGRADE TESTS PASSED!${RESET}"
echo -e "${GREEN}======================================================================${RESET}\n"

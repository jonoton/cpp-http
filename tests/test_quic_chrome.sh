#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

PORT=${1:-4433}
BUILD_DIR="${PROJECT_ROOT}/build"
SERVER_BIN="${BUILD_DIR}/quic_http_server"
LOG_FILE="${SCRIPT_DIR}/quic_chrome_server.log"
CHROME_OUT="${SCRIPT_DIR}/quic_chrome_dom.log"
CHROME_PROFILE_DIR="${SCRIPT_DIR}/quic_chrome_profile"

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

# Determine Chrome binary
if command -v chromium &>/dev/null; then
    CHROME_BIN="chromium"
elif command -v chromium-browser &>/dev/null; then
    CHROME_BIN="chromium-browser"
elif command -v google-chrome &>/dev/null; then
    CHROME_BIN="google-chrome"
else
    echo -e "${RED}ERROR: Chromium / Google Chrome not found in PATH!${RESET}"
    exit 1
fi

print_banner "HTTP/3 (QUIC) Headless Chrome Test Suite"

# Clean up port, profile, and log files
fuser -k ${PORT}/tcp >/dev/null 2>&1 || true
fuser -k ${PORT}/udp >/dev/null 2>&1 || true
rm -f "${LOG_FILE}" "${CHROME_OUT}"
rm -rf "${CHROME_PROFILE_DIR}"
sleep 0.3

# Build server if binary missing or outdated
if [ ! -f "${SERVER_BIN}" ]; then
    echo -e "${YELLOW}Building quic_http_server...${RESET}"
    cmake --build "${BUILD_DIR}" --target quic_http_server -j4
fi

# Calculate SPKI hash of the certificate to override Chromium QUIC cert verification
CERT_DIR="${PROJECT_ROOT}/certs"
CERT_FILE="${CERT_DIR}/server.crt"
SPKI_FLAG=""
if [ -f "${CERT_FILE}" ]; then
    SPKI_HASH=$(openssl x509 -pubkey -in "${CERT_FILE}" | openssl pkey -pubin -outform der | openssl dgst -sha256 -binary | base64)
    echo -e "${BOLD}Dynamically generated SPKI Hash: ${SPKI_HASH}${RESET}"
    SPKI_FLAG="--ignore-certificate-errors-spki-list=${SPKI_HASH}"
fi

echo -e "${BOLD}Starting quic_http_server on port ${PORT}...${RESET}"
${SERVER_BIN} ${PORT} < /dev/null > "${LOG_FILE}" 2>&1 &
SERVER_PID=$!

cleanup() {
    disown ${SERVER_PID} 2>/dev/null || true
    kill -9 ${SERVER_PID} >/dev/null 2>&1 || true
    fuser -k ${PORT}/tcp >/dev/null 2>&1 || true
    fuser -k ${PORT}/udp >/dev/null 2>&1 || true
    rm -rf "${CHROME_PROFILE_DIR}" 2>/dev/null || true
}
trap cleanup EXIT

# Wait for server process output to signal readiness
wait_for_server

# ----------------------------------------------------------------------
# Test 1: Retrieve Home Page DOM using Headless Chrome
# ----------------------------------------------------------------------
print_banner "[TEST 1/3] Fetch Home Page DOM via Headless Chrome"

echo -e "${YELLOW}Running Chrome request (forcing HTTP/3 via QUIC flags)...${RESET}"
${CHROME_BIN} \
    --headless \
    --no-sandbox \
    --disable-gpu \
    --enable-quic \
    --origin-to-force-quic-on=127.0.0.1:${PORT} \
    ${SPKI_FLAG} \
    --ignore-certificate-errors \
    --user-data-dir="${CHROME_PROFILE_DIR}" \
    --dump-dom \
    "https://127.0.0.1:${PORT}/" 2>/dev/null > "${CHROME_OUT}" || true

if grep -q "Hello from HTTP-over-QUIC!" "${CHROME_OUT}"; then
    echo -e "${GREEN}✔ SUCCESS: Headless Chrome fetched Home Page successfully!${RESET}"
else
    echo -e "${RED}✖ ERROR: Headless Chrome failed to fetch Home Page or output incorrect DOM.${RESET}"
    echo "--- Chrome Console Output / DOM ---"
    cat "${CHROME_OUT}"
    echo "--- Server Log Output ---"
    cat "${LOG_FILE}"
    exit 1
fi

# ----------------------------------------------------------------------
# Test 2: Retrieve About Page DOM using Headless Chrome
# ----------------------------------------------------------------------
print_banner "[TEST 2/3] Fetch About Page DOM via Headless Chrome"

rm -f "${CHROME_OUT}"
${CHROME_BIN} \
    --headless \
    --no-sandbox \
    --disable-gpu \
    --enable-quic \
    --origin-to-force-quic-on=127.0.0.1:${PORT} \
    ${SPKI_FLAG} \
    --ignore-certificate-errors \
    --user-data-dir="${CHROME_PROFILE_DIR}" \
    --dump-dom \
    "https://127.0.0.1:${PORT}/about" 2>/dev/null > "${CHROME_OUT}" || true

if grep -q "About This Server" "${CHROME_OUT}"; then
    echo -e "${GREEN}✔ SUCCESS: Headless Chrome fetched About Page successfully!${RESET}"
else
    echo -e "${RED}✖ ERROR: Headless Chrome failed to fetch About Page.${RESET}"
    echo "--- Chrome Console Output / DOM ---"
    cat "${CHROME_OUT}"
    exit 1
fi

# ----------------------------------------------------------------------
# Test 3: Verify QUIC was actually used via /api/stats
# ----------------------------------------------------------------------
print_banner "[TEST 3/3] Verify QUIC usage via API Stats"

if command -v curl-http3 &>/dev/null; then
    CURL_CMD="curl-http3"
else
    CURL_CMD="curl"
fi

STATS_RESPONSE=$(${CURL_CMD} -k -s "https://127.0.0.1:${PORT}/api/stats" || true)

if [ -z "${STATS_RESPONSE}" ]; then
    echo -e "${RED}✖ ERROR: Failed to retrieve API Stats from server.${RESET}"
    exit 1
fi

echo -e "${BOLD}API Stats JSON:${RESET}\n${STATS_RESPONSE}"

# Extract 'streams_opened' count from the json response
STREAMS_OPENED=$(echo "${STATS_RESPONSE}" | grep '"streams_opened":' | sed -E 's/.*"streams_opened":\s*([0-9]+).*/\1/')

if [ -z "${STREAMS_OPENED}" ]; then
    echo -e "${RED}✖ ERROR: Could not parse streams_opened from server stats.${RESET}"
    exit 1
fi

if [ "${STREAMS_OPENED}" -eq 0 ]; then
    echo -e "${RED}✖ ERROR: Headless Chrome did not establish a QUIC connection (streams_opened is 0).${RESET}"
    exit 1
else
    echo -e "${GREEN}✔ SUCCESS: Headless Chrome used HTTP/3 (QUIC)! Total QUIC streams opened: ${STREAMS_OPENED}${RESET}"
fi

echo -e "\n${GREEN}======================================================================${RESET}"
echo -e "${GREEN}  ✔ ALL HEADLESS CHROME HTTP/3 (QUIC) TESTS PASSED!${RESET}"
echo -e "${GREEN}======================================================================${RESET}\n"

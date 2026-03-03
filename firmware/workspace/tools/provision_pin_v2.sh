#!/usr/bin/env bash
set -euo pipefail

HOST_IP="${HOST_IP:-$(/workspace/tools/get_host_ip.sh)}"

write_hex_le() {
  local addr="$1"
  local hex="$2"
  local len="${#hex}"

  # Require whole 32-bit words.
  if (( len % 8 != 0 )); then
    echo "ERROR: hex payload length must be a multiple of 8, got $len" >&2
    exit 1
  fi

  for ((i = 0; i < len; i += 8)); do
    local w="${hex:i:8}"
    local rw
    rw="$(echo "$w" | sed 's/\(..\)/\1 /g' | awk '{for(i=NF;i>=1;i--) printf "%s",$i}')"
    local rw_uc
    rw_uc="$(echo "$rw" | tr '[:lower:]' '[:upper:]')"
    nrfjprog --ip "$HOST_IP" -f nrf52 --memwr "$addr" --val "0x$rw"
    local rd
    rd="$(nrfjprog --ip "$HOST_IP" -f nrf52 --memrd "$addr" --n 4 | awk '/^0x/ {print toupper($2); exit}')"
    if [[ "$rd" != "$rw_uc" ]]; then
      printf "ERROR: word verify failed at addr=0x%08X wrote=%s read=%s\n" "$addr" "$rw_uc" "$rd" >&2
      exit 1
    fi
    addr=$((addr + 4))
  done
}

read_hex_le() {
  local addr="$1"
  local nbytes="$2"
  nrfjprog --ip "$HOST_IP" -f nrf52 --memrd "$addr" --n "$nbytes" | awk '
  /^0x/ {
    for (i=2; i<=NF; i++) {
      gsub(/[^0-9A-Fa-f]/, "", $i)
      if (length($i)==8) {
        printf substr($i,7,2) substr($i,5,2) substr($i,3,2) substr($i,1,2)
      }
    }
  }
  END { print "" }'
}

KEY_HEX="$(openssl rand -hex 32 | tr '[:lower:]' '[:upper:]')"
PIN=825852
CT_HEX="$(printf "%s" "$PIN" | openssl enc -aes-256-cbc -nosalt -K "$KEY_HEX" -iv 00000000000000000000000000000000 | xxd -p -c 256 | tr -d '\r\n' | tr '[:lower:]' '[:upper:]')"

echo "Using HOST_IP=$HOST_IP"
echo "KEY len=${#KEY_HEX} CT len=${#CT_HEX}"

if [[ ${#KEY_HEX} -ne 64 || ${#CT_HEX} -ne 32 ]]; then
  echo "ERROR: unexpected key/cipher lengths." >&2
  echo "KEY_HEX=$KEY_HEX" >&2
  echo "CT_HEX=$CT_HEX" >&2
  exit 1
fi

nrfjprog --ip "$HOST_IP" -f nrf52 --erasepage 0x000FD000
nrfjprog --ip "$HOST_IP" -f nrf52 --halt
write_hex_le 0x000FD000 "$KEY_HEX"
write_hex_le 0x000FD020 "$CT_HEX"

KEY_RD="$(read_hex_le 0x000FD000 32)"
CT_RD="$(read_hex_le 0x000FD020 16)"

echo "KEY_RD=$KEY_RD"
echo "CT_RD=$CT_RD"

if [[ "$KEY_RD" != "$KEY_HEX" || "$CT_RD" != "$CT_HEX" ]]; then
  echo "ERROR: flash readback mismatch." >&2
  echo "KEY_HEX=$KEY_HEX" >&2
  echo "CT_HEX=$CT_HEX" >&2
  exit 1
fi

nrfjprog --ip "$HOST_IP" -f nrf52 --reset
echo "Provisioned and verified successfully."

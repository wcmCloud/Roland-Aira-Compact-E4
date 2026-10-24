#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
dotnet run --project Tools/IconRenderer -c Release -- assets/icon

#!/bin/bash
set -e

pip3 install --no-cache-dir --ignore-installed --force-reinstall wheel pyyaml requests
pip3 install --no-cache-dir --ignore-installed --force-reinstall fetchai-ledger-api==0.8.0a2
pip3 install --no-cache-dir --ignore-installed --force-reinstall ./scripts/fetchai_netutils

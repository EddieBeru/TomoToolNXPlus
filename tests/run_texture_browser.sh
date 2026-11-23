#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
texture_test_dir=$(mktemp -d /tmp/tomotool-browser-tests.XXXXXX)
trap 'rm -rf -- "$texture_test_dir"' EXIT
flags=(-std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -Iinclude)
if [[ "${SANITIZE:-0}" == 1 ]]; then
    flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
fi
run_test() {
    local name=$1
    shift
    g++ "${flags[@]}" "$@" -o "$texture_test_dir/$name"
    "$texture_test_dir/$name"
}
run_test decode source/texture_decode.cpp tests/texture_decode_test.cpp -lzstd
run_test scanner source/ugc_scanner.cpp tests/ugc_scanner_test.cpp
run_test names source/ugc_names.cpp tests/ugc_names_test.cpp
run_test browser source/texture_browser.cpp source/ugc_scanner.cpp source/ugc_names.cpp tests/texture_browser_test.cpp
run_test preview source/texture_decode.cpp source/texture_browser_preview.cpp tests/texture_browser_preview_test.cpp -lzstd
run_test session -Iapps/borealis/source apps/borealis/source/texture_browser_session.cpp tests/texture_browser_session_test.cpp -pthread
run_test read-source -Itests/support -Iapps/borealis/source apps/borealis/source/texture_read_source.cpp tests/texture_read_source_test.cpp

run_test profile-selector -Itests/support -Iapps/borealis/source apps/borealis/source/profile_selector.cpp tests/profile_selector_test.cpp

run_test integration -Itests/support -Iapps/borealis/source apps/borealis/source/texture_read_source.cpp apps/borealis/source/texture_browser_session.cpp source/texture_browser.cpp source/ugc_scanner.cpp source/ugc_names.cpp source/texture_browser_preview.cpp source/texture_decode.cpp tests/texture_browser_integration_test.cpp -pthread -lzstd
run_test player-profile source/player_profile.cpp tests/player_profile_test.cpp
run_test player-context source/player_profile.cpp tests/player_browser_context_test.cpp
run_test ui-screens -frtti -fexceptions -Itests/support/borealis_navigation -Itests/support -Iapps/borealis/source apps/borealis/source/app.cpp apps/borealis/source/app_chrome.cpp apps/borealis/source/exit_menu.cpp apps/borealis/source/navigation.cpp apps/borealis/source/player_browser_screen.cpp apps/borealis/source/profile_header.cpp apps/borealis/source/profile_selector.cpp apps/borealis/source/texture_browser_screen.cpp apps/borealis/source/texture_browser_session.cpp apps/borealis/source/texture_preview_view.cpp apps/borealis/source/texture_read_source.cpp apps/borealis/source/texture_settings_screen.cpp source/focus_navigation.cpp source/player_profile.cpp source/texture_browser.cpp source/texture_browser_preview.cpp source/texture_decode.cpp source/texture_settings.cpp source/texture_settings_config.cpp source/texture_settings_store.cpp source/ugc_names.cpp source/ugc_scanner.cpp tests/borealis_ui_screens_test.cpp -pthread -lzstd

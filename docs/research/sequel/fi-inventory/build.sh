#!/bin/zsh
source ~/VulkanSDK/1.4.363.0/setup-env.sh >/dev/null
cd /Users/matteo/dev/kya/.claude/worktrees/agent-a0cbedbfa6600eab6
[ -d out/build/macos-debug ] || cmake --preset macos-debug > $1/cfg.log 2>&1
cmake --build out/build/macos-debug -j 12 > $1/build.log 2>&1
echo exit $?

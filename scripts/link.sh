# load brew into this shell (Apple Silicon + Intel fallback)
if [ -x /opt/homebrew/bin/brew ]; then
  eval "$(/opt/homebrew/bin/brew shellenv)"
elif [ -x /usr/local/bin/brew ]; then
  eval "$(/usr/local/bin/brew shellenv)"
fi

brew update
brew install mingw-w64 cmake ninja

# verify
x86_64-w64-mingw32-g++ --version
x86_64-w64-mingw32-gcc --version
x86_64-w64-mingw32-windres --version

# build
cd /Users/proteu5/Documents/Github/RefPad
/Users/proteu5/Documents/Github/RefPad/scripts/build-win.sh 24

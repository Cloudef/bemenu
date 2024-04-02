{
  lib
  , stdenv
  , pkg-config
  , scdoc
  , ncurses
  , cairo
  , fribidi
  , harfbuzz
  , libxkbcommon
  , pango
  , wayland
  , wayland-scanner
  , wayland-protocols
  , xorg
}:

with builtins;
with lib;

let
  version = readFile ./VERSION;
in stdenv.mkDerivation {
  src = with fileset; toSource {
    root = ./.;
    fileset = unions [ ./VERSION ./GNUmakefile ./bemenu.pc.in ./scripts ./lib ./man ./client ];
  };
  inherit version;
  pname = "bemenu";

  strictDeps = true;
  nativeBuildInputs = [
    pkg-config
    scdoc
  ] ++ optionals (stdenv.isLinux) [
    wayland-scanner
  ];

  buildInputs = [
    ncurses
  ] ++ optionals (stdenv.isLinux) [
    cairo
    fribidi
    harfbuzz
    libxkbcommon
    pango
    # Wayland
    wayland wayland-protocols
    # X11
    xorg.libX11 xorg.libXinerama xorg.libXft
    xorg.libXdmcp xorg.libpthreadstubs xorg.libxcb
  ];

  postPatch = "" + optionalString (stdenv.isDarwin) ''
    substituteInPlace GNUmakefile --replace '-soname' '-install_name'
  '';

  makeFlags = [ "PREFIX=$(out)" ];
  buildFlags = [ "PREFIX=$(out)" "clients" "curses" ] ++ optionals (stdenv.isLinux) [ "wayland" "x11" ];

  # https://github.com/NixOS/nixpkgs/blob/master/pkgs/build-support/setup-hooks/fix-darwin-dylib-names.sh
  # ^ does not handle .so files
  postInstall = "" + optionalString (stdenv.isDarwin) ''
    so="$(find "$out/lib" -name "libbemenu.so.[0-9]" -print -quit)"
    for f in "$out/bin/"*; do
        install_name_tool -change "$(basename $so)" "$so" $f
    done
  '';

  doCheck = stdenv.isLinux;
  checkPhase = ''
    make check-symbols
  '';

  meta = {
    homepage = "https://github.com/Cloudef/bemenu";
    description = "Dynamic menu library and client program inspired by dmenu";
    license = licenses.gpl3Plus;
    platforms = with platforms; darwin ++ linux;
    mainProgram = "bemenu-run";
  };
}

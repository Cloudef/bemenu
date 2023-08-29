{ pkgs ? import <nixpkgs> {}, lib ? pkgs.lib, stdenv ? pkgs.stdenv }:

let
  src = pkgs.copyPathToStore ./.;
  semver = builtins.readFile "${src}/VERSION";
  revision = builtins.readFile (pkgs.runCommand "get-rev" {
    nativeBuildInputs = with pkgs; [ git ];
  } "GIT_DIR=${src}/.git git rev-parse --short HEAD | tr -d '\n' > $out");
in stdenv.mkDerivation {
  inherit src;
  pname = "bemenu";
  version = "${semver}${revision}";

  strictDeps = true;
  nativeBuildInputs = with pkgs; [
    pkg-config
    scdoc
  ] ++ lib.optionals (stdenv.isLinux) [
    wayland-scanner
  ];

  buildInputs = with pkgs; [
    ncurses
  ] ++ lib.optionals (stdenv.isLinux) [
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

  postPatch = "" + lib.optionalString (stdenv.isDarwin) ''
    substituteInPlace GNUmakefile --replace '-soname' '-install_name'
  '';

  makeFlags = [ "PREFIX=$(out)" ];
  buildFlags = [ "PREFIX=$(out)" "clients" "curses" ] ++ lib.optionals (stdenv.isLinux) [ "wayland" "x11" ];

  # https://github.com/NixOS/nixpkgs/blob/master/pkgs/build-support/setup-hooks/fix-darwin-dylib-names.sh
  # ^ does not handle .so files
  postInstall = "" + lib.optionalString (stdenv.isDarwin) ''
    so="$(find "$out/lib" -name "libbemenu.so.[0-9]" -print -quit)"
    for f in "$out/bin/"*; do
        install_name_tool -change "$(basename $so)" "$so" $f
    done
  '';

  meta = with pkgs.lib; {
    homepage = "https://github.com/Cloudef/bemenu";
    description = "Dynamic menu library and client program inspired by dmenu";
    license = licenses.gpl3Plus;
    platforms = with platforms; darwin ++ linux;
  };
}

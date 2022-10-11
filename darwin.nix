{ pkgs ? import <nixpkgs> {} }:

let
  src = pkgs.copyPathToStore ./.;
  semver = builtins.readFile "${src}/VERSION";
  revision = builtins.readFile (pkgs.runCommand "get-rev" {
    nativeBuildInputs = with pkgs; [ git ];
  } "GIT_DIR=${src}/.git git rev-parse --short HEAD | tr -d '\n' > $out");
in pkgs.stdenv.mkDerivation rec {
  inherit src;
  pname = "bemenu";
  version = "${semver}${revision}";
  nativeBuildInputs = with pkgs; [ pkg-config scdoc ];
  buildInputs = with pkgs; [ ncurses ];

  postPatch = ''
    substituteInPlace GNUmakefile --replace '-soname' '-install_name'
  '';

  makeFlags = ["PREFIX=$(out)"];
  buildFlags = ["PREFIX=$(out)" "clients" "curses"];

  # https://github.com/NixOS/nixpkgs/blob/master/pkgs/build-support/setup-hooks/fix-darwin-dylib-names.sh
  # ^ does not handle .so files
  postInstall = ''
    so="$(find "$out/lib" -name "libbemenu.so.[0-9]" -print -quit)"
    for f in "$out/bin/"*; do
        install_name_tool -change "$(basename $so)" "$so" $f
    done
  '';

  meta = with pkgs.lib; {
    homepage = "https://github.com/Cloudef/bemenu";
    description = "Dynamic menu library and client program inspired by dmenu";
    license = licenses.gpl3Plus;
    platforms = with platforms; darwin;
  };
}

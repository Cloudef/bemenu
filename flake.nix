{
   description = "bemenu";

   inputs = {
      nixpkgs.url = "github:nixos/nixpkgs";
      flake-utils.url = "github:numtide/flake-utils";
   };

   outputs = { self, nixpkgs, flake-utils }: let
      outputs = (flake-utils.lib.eachDefaultSystem (system: let
         pkgs = nixpkgs.outputs.legacyPackages.${system};
      in {
         packages.default = pkgs.callPackage ./default.nix {};
      }));
   in outputs;
}

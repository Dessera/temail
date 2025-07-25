{
  description = "Email client written in Qt.";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs =
    { flake-parts, ... }@inputs:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [ "x86_64-linux" ];

      perSystem =
        {
          self',
          pkgs,
          ...
        }:
        let
          stdenv = pkgs.gcc14Stdenv;

          clang-tools = pkgs.callPackage ./.nix-support/clang-tools.nix {
            inherit stdenv;
          };
        in
        {
          packages.default = pkgs.qt6.callPackage ./default.nix { inherit stdenv; };
          devShells.default =
            pkgs.mkShell.override
              {
                inherit stdenv;
              }
              {
                inputsFrom = [ self'.packages.default ];
                hardeningDisable = [ "fortify" ];

                packages =
                  (with pkgs; [
                    nixd
                    nixfmt-rfc-style
                    mesonlsp
                    doxygen
                  ])
                  ++ [
                    clang-tools
                  ];
              };
        };
    };
}

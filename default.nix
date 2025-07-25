{
  stdenv,
  lib,

  # Build tools
  meson,
  ninja,
  pkg-config,

  # Deps
  qtbase,
  wrapQtAppsHook,
}:
stdenv.mkDerivation {
  pname = "temail";
  version = "0.1.0";
  src = lib.cleanSource ./.;

  buildInputs = [ qtbase ];

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    wrapQtAppsHook
  ];
}

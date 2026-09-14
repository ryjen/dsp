{
  description = "ryjen/dsp — realtime guitar DSP experiments";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs =
    { self, nixpkgs }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      checks = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          native = pkgs.stdenv.mkDerivation {
            pname = "dsp-native-check";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
            ];

            configurePhase = ''
              runHook preConfigure
              cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
              runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild
              cmake --build build
              runHook postBuild
            '';

            doCheck = true;
            checkPhase = ''
              runHook preCheck
              ctest --test-dir build --output-on-failure
              runHook postCheck
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p "$out"
              touch "$out/native-check-passed"
              runHook postInstall
            '';
          };

          faust-generated =
            pkgs.runCommand "dsp-faust-generated-check"
              {
                nativeBuildInputs = [
                  pkgs.faust
                  pkgs.diffutils
                  pkgs.which
                ];
              }
              ''
                cp -R ${self} source
                chmod -R +w source
                cd source
                sh ./tools/generate-faust.sh "$TMPDIR/tremolo_faust.hpp"
                diff -u effects/tremolo/generated/tremolo_faust.hpp "$TMPDIR/tremolo_faust.hpp"
                touch "$out"
              '';
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          default = pkgs.mkShell {
            packages = [
              pkgs.clang
              pkgs.clang-tools
              pkgs.cmake
              pkgs.coreutils
              pkgs.git
              pkgs.ninja
              pkgs.faust
            ];
          };
        }
      );

      formatter = forAllSystems (system: (import nixpkgs { inherit system; }).nixfmt);
    };
}

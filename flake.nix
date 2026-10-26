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
      # Distributable native offline renderer, intentionally separate from
      # JUCE, LV2 and Daisy hardware/toolchain builds. The normal GitHub
      # action checks are not shipped as part of the executable.
      packages = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        rec {
          dsp-render = pkgs.stdenv.mkDerivation {
            pname = "micrantha-dsp-render";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];
            configurePhase = ''
              runHook preConfigure
              cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
              runHook postConfigure
            '';
            buildPhase = ''
              runHook preBuild
              cmake --build build --target dsp_render_cli
              runHook postBuild
            '';
            doCheck = true;
            checkPhase = ''
              runHook preCheck
              cmake --build build --target dsp_effects_audio_demo
              ctest --test-dir build -R '^dsp_render_cli_test
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          armToolchain = pkgs.gcc-arm-embedded;
          libdaisy = pkgs.fetchgit {
            url = "https://github.com/daisyaudio/libDaisy.git";
            rev = "facb66c76b5482918741695f4268b0185e474644";
            hash = "sha256-yQ7Xdsa1RnSLK6TJEHKq7xjJTe7KiG7pjA2My+3RUNU=";
            fetchSubmodules = true;
          };
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

          strict = pkgs.stdenv.mkDerivation {
            pname = "dsp-strict-check";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
            ];

            configurePhase = ''
              runHook preConfigure
              cmake -S . -B build -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror"
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
              touch "$out/strict-check-passed"
              runHook postInstall
            '';
          };

          target-boundary = pkgs.runCommand "dsp-target-boundary-check" { nativeBuildInputs = [ pkgs.bash pkgs.gnugrep ]; } ''
            cd ${self}
            bash ./tools/check-target-boundary.sh
            touch "$out"
          '';

          pistomp-scripts =
            pkgs.runCommand "dsp-pistomp-script-syntax-check"
              { nativeBuildInputs = [ pkgs.bash ]; }
              ''
                bash -n ${self}/targets/pistomp/build-bundles.sh
                bash -n ${self}/targets/pistomp/install-bundles.sh
                touch "$out"
              '';
          juce-boundary =
            pkgs.runCommand "dsp-juce-boundary-check"
              { nativeBuildInputs = [ pkgs.bash pkgs.gnugrep ]; }
              ''
                cd ${self}
                bash ./tools/check-juce-boundary.sh
                touch "$out"
              '';

          juce-host = pkgs.stdenv.mkDerivation {
            pname = "dsp-juce-host-check";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
            ];
            buildInputs = [ pkgs.juce ];

            configurePhase = ''
              runHook preConfigure
              cmake -S . -B build -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DDSP_ENABLE_JUCE_HOST=ON
              runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild
              cmake --build build --target dsp_juce_host_test
              runHook postBuild
            '';

            doCheck = true;
            checkPhase = ''
              runHook preCheck
              ctest --test-dir build -R dsp_juce_host_test --output-on-failure
              runHook postCheck
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p "$out"
              touch "$out/juce-host-check-passed"
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
        // pkgs.lib.optionalAttrs pkgs.stdenv.isLinux {

          lv2-host = pkgs.stdenv.mkDerivation {
            pname = "dsp-lv2-host-check";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.lv2 pkgs.lilv ];
            configurePhase = ''
              runHook preConfigure
              cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DDSP_ENABLE_LV2_HOST=ON
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
              export LV2_PATH="$PWD/build/lv2"
              lv2ls | grep -Fx 'https://micrantha.com/lv2/delay'
              lv2ls | grep -Fx 'https://micrantha.com/lv2/tremolo'
              runHook postCheck
            '';
            installPhase = ''
              runHook preInstall
              mkdir -p "$out"
              touch "$out/lv2-host-check-passed"
              runHook postInstall
            '';
          };
          daisy-firmware = pkgs.stdenv.mkDerivation {
            pname = "dsp-daisy-firmware-check";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
              armToolchain
            ];

            configurePhase = ''
              runHook preConfigure
              cmake -S targets/daisy -B build-daisy -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_MAKE_PROGRAM=${pkgs.ninja}/bin/ninja \
                -DDAISY_ARM_GCC=${armToolchain}/bin/arm-none-eabi-gcc \
                -DLIBDAISY_DIR=${libdaisy} \
                -DCMAKE_TOOLCHAIN_FILE=${self}/cmake/toolchains/daisy-arm-none-eabi.cmake
              runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild
              cmake --build build-daisy --target hothouse_delay
              runHook postBuild
            '';

            doCheck = true;
            checkPhase = ''
              runHook preCheck
              ${armToolchain}/bin/arm-none-eabi-size build-daisy/hothouse_delay.elf
              ${armToolchain}/bin/arm-none-eabi-readelf -h build-daisy/hothouse_delay.elf > elf-header.txt
              grep -q 'Machine:.*ARM' elf-header.txt
              test -s build-daisy/hothouse_delay.bin
              grep -q 'sample_rate_hz = 48000.0' targets/daisy/firmware/hothouse_config.hpp
              grep -q 'block_size = 48' targets/daisy/firmware/hothouse_config.hpp
              runHook postCheck
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p "$out"
              cp build-daisy/hothouse_delay.elf "$out/"
              cp build-daisy/hothouse_delay.bin "$out/"
              runHook postInstall
            '';
          };

          sanitizers = pkgs.clangStdenv.mkDerivation {
            pname = "dsp-sanitizer-check";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
            ];

            configurePhase = ''
              runHook preConfigure
              cmake -S . -B build -G Ninja \
                -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
                -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
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
              touch "$out/sanitizer-check-passed"
              runHook postInstall
            '';
          };
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


          lv2 = pkgs.mkShell {
            packages = [ pkgs.clang pkgs.cmake pkgs.ninja pkgs.lv2 pkgs.lilv ];
          };
          juce = pkgs.mkShell {
            packages = [
              pkgs.clang
              pkgs.clang-tools
              pkgs.cmake
              pkgs.coreutils
              pkgs.git
              pkgs.ninja
              pkgs.pkg-config
              pkgs.faust
              pkgs.juce
            ];
          };
        }
      );

      formatter = forAllSystems (system: (import nixpkgs { inherit system; }).nixfmt);
    };
}
 --output-on-failure
              runHook postCheck
            '';
            installPhase = ''
              runHook preInstall
              install -Dm755 build/dsp "$out/bin/dsp"
              runHook postInstall
            '';
          };
          default = dsp-render;
        }
      );

      checks = forAllSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          armToolchain = pkgs.gcc-arm-embedded;
          libdaisy = pkgs.fetchgit {
            url = "https://github.com/daisyaudio/libDaisy.git";
            rev = "facb66c76b5482918741695f4268b0185e474644";
            hash = "sha256-yQ7Xdsa1RnSLK6TJEHKq7xjJTe7KiG7pjA2My+3RUNU=";
            fetchSubmodules = true;
          };
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

          strict = pkgs.stdenv.mkDerivation {
            pname = "dsp-strict-check";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
            ];

            configurePhase = ''
              runHook preConfigure
              cmake -S . -B build -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror"
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
              touch "$out/strict-check-passed"
              runHook postInstall
            '';
          };

          target-boundary = pkgs.runCommand "dsp-target-boundary-check" { nativeBuildInputs = [ pkgs.bash pkgs.gnugrep ]; } ''
            cd ${self}
            bash ./tools/check-target-boundary.sh
            touch "$out"
          '';

          pistomp-scripts =
            pkgs.runCommand "dsp-pistomp-script-syntax-check"
              { nativeBuildInputs = [ pkgs.bash ]; }
              ''
                bash -n ${self}/targets/pistomp/build-bundles.sh
                bash -n ${self}/targets/pistomp/install-bundles.sh
                touch "$out"
              '';
          juce-boundary =
            pkgs.runCommand "dsp-juce-boundary-check"
              { nativeBuildInputs = [ pkgs.bash pkgs.gnugrep ]; }
              ''
                cd ${self}
                bash ./tools/check-juce-boundary.sh
                touch "$out"
              '';

          juce-host = pkgs.stdenv.mkDerivation {
            pname = "dsp-juce-host-check";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
            ];
            buildInputs = [ pkgs.juce ];

            configurePhase = ''
              runHook preConfigure
              cmake -S . -B build -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DDSP_ENABLE_JUCE_HOST=ON
              runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild
              cmake --build build --target dsp_juce_host_test
              runHook postBuild
            '';

            doCheck = true;
            checkPhase = ''
              runHook preCheck
              ctest --test-dir build -R dsp_juce_host_test --output-on-failure
              runHook postCheck
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p "$out"
              touch "$out/juce-host-check-passed"
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
        // pkgs.lib.optionalAttrs pkgs.stdenv.isLinux {

          lv2-host = pkgs.stdenv.mkDerivation {
            pname = "dsp-lv2-host-check";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.lv2 pkgs.lilv ];
            configurePhase = ''
              runHook preConfigure
              cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DDSP_ENABLE_LV2_HOST=ON
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
              export LV2_PATH="$PWD/build/lv2"
              lv2ls | grep -Fx 'https://micrantha.com/lv2/delay'
              lv2ls | grep -Fx 'https://micrantha.com/lv2/tremolo'
              runHook postCheck
            '';
            installPhase = ''
              runHook preInstall
              mkdir -p "$out"
              touch "$out/lv2-host-check-passed"
              runHook postInstall
            '';
          };
          daisy-firmware = pkgs.stdenv.mkDerivation {
            pname = "dsp-daisy-firmware-check";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
              armToolchain
            ];

            configurePhase = ''
              runHook preConfigure
              cmake -S targets/daisy -B build-daisy -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_MAKE_PROGRAM=${pkgs.ninja}/bin/ninja \
                -DDAISY_ARM_GCC=${armToolchain}/bin/arm-none-eabi-gcc \
                -DLIBDAISY_DIR=${libdaisy} \
                -DCMAKE_TOOLCHAIN_FILE=${self}/cmake/toolchains/daisy-arm-none-eabi.cmake
              runHook postConfigure
            '';

            buildPhase = ''
              runHook preBuild
              cmake --build build-daisy --target hothouse_delay
              runHook postBuild
            '';

            doCheck = true;
            checkPhase = ''
              runHook preCheck
              ${armToolchain}/bin/arm-none-eabi-size build-daisy/hothouse_delay.elf
              ${armToolchain}/bin/arm-none-eabi-readelf -h build-daisy/hothouse_delay.elf > elf-header.txt
              grep -q 'Machine:.*ARM' elf-header.txt
              test -s build-daisy/hothouse_delay.bin
              grep -q 'sample_rate_hz = 48000.0' targets/daisy/firmware/hothouse_config.hpp
              grep -q 'block_size = 48' targets/daisy/firmware/hothouse_config.hpp
              runHook postCheck
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p "$out"
              cp build-daisy/hothouse_delay.elf "$out/"
              cp build-daisy/hothouse_delay.bin "$out/"
              runHook postInstall
            '';
          };

          sanitizers = pkgs.clangStdenv.mkDerivation {
            pname = "dsp-sanitizer-check";
            version = "0.1.0";
            src = self;
            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
            ];

            configurePhase = ''
              runHook preConfigure
              cmake -S . -B build -G Ninja \
                -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
                -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
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
              touch "$out/sanitizer-check-passed"
              runHook postInstall
            '';
          };
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


          lv2 = pkgs.mkShell {
            packages = [ pkgs.clang pkgs.cmake pkgs.ninja pkgs.lv2 pkgs.lilv ];
          };
          juce = pkgs.mkShell {
            packages = [
              pkgs.clang
              pkgs.clang-tools
              pkgs.cmake
              pkgs.coreutils
              pkgs.git
              pkgs.ninja
              pkgs.pkg-config
              pkgs.faust
              pkgs.juce
            ];
          };
        }
      );

      formatter = forAllSystems (system: (import nixpkgs { inherit system; }).nixfmt);
    };
}

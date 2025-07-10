{
  description = "PowerPlay - Home Energy Management";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
  };
  outputs = { self, nixpkgs, flake-utils }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};

      version = "0.1.0";

      nativeBuildInputs = with pkgs; [
        makeWrapper
        pkg-config
      ];

      buildInputs = with pkgs; [
        libmodbus
      ];
    in
      {
        nixosModules.powerplay = { config, lib, pkgs, ... }: {
          options.services = {
            powerplay = {
              package = lib.mkOption {
                type = lib.types.package;
                default = self.packages.${pkgs.system}.default;
                description = "Package to use for powerplay";
              };
              gxHost = lib.mkOption {
                type = lib.types.str;
                default = "192.168.1.14";
                description = "GX device host";
              };
              gxPort = lib.mkOption {
                type = lib.types.port;
                default = 502;
                description = "GX device Modbus port";
              };
              evcsHost = lib.mkOption {
                type = lib.types.str;
                default = "192.168.1.23";
                description = "EVCS host";
              };
              evcsPort = lib.mkOption {
                type = lib.types.port;
                default = 502;
                description = "EVCS Modbus port";
              };

              sparkshift = {
                enable = lib.mkEnableOption "sparkshift charge controller";
                averagingSecs = lib.mkOption {
                  type = lib.types.int;
                  default = 300;
                  description = "Power averaging seconds";
                };
                sleepSecs = lib.mkOption {
                  type = lib.types.int;
                  default = 1;
                  description = "Control loop sleep seconds";
                };
                powerExcessMin = lib.mkOption {
                  type = lib.types.int;
                  default = (230*3*6) + 500 + 250;
                  description = "Minimum excess power to enable charging";
                };
                debug = lib.mkOption {
                  type = lib.types.bool;
                  default = false;
                  description = "Log additional debug information";
                };
                dryrun = lib.mkOption {
                  type = lib.types.bool;
                  default = false;
                  description = "Only collect information, no not manage charging";
                };
              };
            };
          };

          config = {
            systemd.services = lib.mkMerge [
              (lib.mkIf config.services.powerplay.sparkshift.enable {
                sparkshift = {
                  description = "Sparkshift charge controller";
                  wantedBy = [ "multi-user.target" ];
                  after = [ "network.target" ];
                  serviceConfig = {
                    ExecStart = "${config.services.powerplay.package}/bin/sparkshift";
                    Environment = [
                      "GX_HOST=${config.services.powerplay.gxHost}"
                      "GX_PORT=${toString config.services.powerplay.gxPort}"
                      "EVCS_HOST=${config.services.powerplay.evcsHost}"
                      "EVCS_PORT=${toString config.services.powerplay.evcsPort}"
                      "POWER_EXCESS_MIN=${toString config.services.powerplay.sparkshift.powerExcessMin}"
                      "SLEEP_SECS=${toString config.services.powerplay.sparkshift.sleepSecs}"
                      "AVERAGING_SECS=${toString config.services.powerplay.sparkshift.averagingSecs}"
                      "SPARKSHIFT_DEBUG=${toString config.services.powerplay.sparkshift.debug}"
                      "SPARKSHIFT_DRYRUN=${toString config.services.powerplay.sparkshift.dryrun}"
                    ];
                    Restart = "always";
                    RestartSec = "30";

                    Type = "simple";

                    DynamicUser = true;
                    ReadOnlyPaths = "/";
                    RestrictAddressFamilies = [ "AF_INET" "AF_INET6" ];
                    CapabilityBoundingSet = "";
                    SystemCallFilter = [
                      "@basic-io"
                      "@file-system"
                      "@io-event"
                      "@network-io"
                    ];
                    SystemCallArchitectures = "native";
                  };
                };
              })
            ];
          };
        };

        packages.${system} = {
          default = pkgs.stdenv.mkDerivation {
          inherit buildInputs nativeBuildInputs version;

          pname = "powerplay";
          src = ./.;
          };
        };

        devShells.${system}.default = pkgs.mkShell {
          inherit buildInputs nativeBuildInputs;

          GX_HOST = "192.168.2.21";
          GX_PORT = 502;
          EVCS_HOST = "192.168.2.22";
          EVCS_PORT = 502;
          POWER_EXCESS_MIN = 5000;
          AVERAGING_SECS = 30;
          SLEEP_SECS = 3;
          SPARKSHIFT_DEBUG = 1;
          SPARKSHIFT_DRYRUN = 1;
        };
      };
}

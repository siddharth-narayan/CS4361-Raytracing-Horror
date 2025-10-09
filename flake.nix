{
  inputs.openssl-quantum.url = "github:siddharth-narayan/openssl-with-providers";
  outputs =
    { nixpkgs, openssl-quantum, ... }:
    let
      pkgs = import nixpkgs { system = "x86_64-linux"; };
    in
    {
      devShells.x86_64-linux.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          meson
          ninja
          raylib
        ];
      };
    };
}
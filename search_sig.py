import subprocess, sys
from cryptography.hazmat.primitives.asymmetric import ed25519

pub = ed25519.Ed25519PublicKey.from_public_bytes(bytes.fromhex('489532082ae4dfc21c6ffe21e1bf78c432bc07200d712ad07568c9a46fe52f24'))

# Get all commits
commits = subprocess.check_output(['git', 'rev-list', 'HEAD']).decode().strip().split('\n')

for commit in commits:
    try:
        data = subprocess.check_output(['git', 'show', f'{commit}:operator-signed-integrity-manifest-hashes.json'], stderr=subprocess.DEVNULL)
        sig_hex = subprocess.check_output(['git', 'show', f'{commit}:operator-signed-integrity-manifest-hashes.sig'], stderr=subprocess.DEVNULL).decode().strip()
        pub.verify(bytes.fromhex(sig_hex), data)
        print(f"FOUND VERIFIED COMMIT: {commit}")
        sys.exit(0)
    except Exception:
        pass

print("NO VERIFIED COMMIT FOUND")
sys.exit(1)

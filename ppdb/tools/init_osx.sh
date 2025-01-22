
# https://github.com/jart/cosmopolitan/tree/master

##set REPO_DIR=$(cd $0)

# dir of this tools/
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo SCRIPT_DIR=$SCRIPT_DIR
cd "$SCRIPT_DIR/../.."
ROOT_DIR="$(pwd)"
echo ROOT_DIR=$ROOT_DIR

cd "$ROOT_DIR/repos/"
REPO_DIR="$(pwd)"
echo REPO_DIR=$REPO_DIR

cd "$REPO_DIR"
pwd
mkdir -p cosmocc
cd cosmocc
# https://github.com/jart/cosmopolitan/releases/download/4.0.2/cosmocc-4.0.2.zip
curl -C - -L -o cosmocc.zip https://cosmo.zip/pub/cosmocc/cosmocc.zip
ls -al cosmocc.zip
unzip cosmocc.zip

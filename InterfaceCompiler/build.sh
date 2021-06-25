if [ -z $CXX ]; then
	CXX=g++
fi

echo "Building $(pwd)/lic"
$CXX -o lic -std=c++17 main.cpp -Wall -Wextra

if [ -z $CXX ]; then
	CXX=g++
fi

$CXX -o lic -std=c++17 main.cpp -Wall -Wextra

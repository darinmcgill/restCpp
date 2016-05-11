set -e 
clang++ -std=c++11 serve.cpp -o serve.out 
clang++ -std=c++11 request.cpp -o request.out

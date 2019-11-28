# According to the assignment,

# "make all" compiles all files and creates executables
all:
	g++ -std=c++11 -O3 -o client client.cpp
	g++ -std=c++11 -O3 -o aws aws.cpp
	g++ -std=c++11 -O3 -o serverB serverB.cpp
	g++ -std=c++11 -O3 -o serverA serverA.cpp

# "make serverA" runs server A, rather than compile serverA
.PHONY: serverA
serverA:
	./serverA

# "make serverB" runs server B, rather than compile serverB
.PHONY: serverB
serverB:
	./serverB

# "make aws" runs main server, rather than compile aws
.PHONY: aws
aws:
	./aws

clean: 
	$(RM) client
	$(RM) aws
	$(RM) serverB
	$(RM) serverA

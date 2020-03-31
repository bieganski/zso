CXX := g++
CXXFLAGS := -O0 -fno-common -no-pie -fno-pie -export-dynamic
TARGET := postlinker
LIBS := SectionEditor.cpp Utils.cpp

all: solution

solution:
	$(CXX) $(CXXFLAGS) $(LIBS) solution.cpp -o $(TARGET)
	cp postlinker z1/

clean:
	rm -f $(TARGET) 

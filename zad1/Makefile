CXX := g++
CXXFLAGS := -O2 -fno-common -no-pie -fno-pie -export-dynamic
TARGET := solution
LIBS := SectionEditor.cpp Utils.cpp

all: solution

solution:
	$(CXX) $(CXXFLAGS) $(LIBS) $(TARGET).cpp -o $(TARGET)

clean:
	rm -f $(TARGET) 

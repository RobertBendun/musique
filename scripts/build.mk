Release_Obj=$(addprefix bin/$(os)/,$(Obj)) bin/$(os)/builtin_function_documentation.o

# bin/$(os)/libreplxx.a:
# 	@CXX=$(CXX) os=$(os) scripts/build_replxx.sh

bin/$(os)/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $< -c

bin/$(os)/$(Target): $(Release_Obj) bin/$(os)/main.o bin/$(os)/rtmidi.o
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $(shell CXX=$(CXX) os=$(os) scripts/build_replxx.sh)  $(Release_Obj) bin/$(os)/rtmidi.o $(LDFLAGS) $(LDLIBS)

bin/$(os)/builtin_function_documentation.o: bin/$(os)/builtin_function_documentation.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $< -c

bin/$(os)/builtin_function_documentation.cc: musique/interpreter/builtin_functions.cc scripts/document-builtin.py
	scripts/document-builtin.py -f cpp -o $@ musique/interpreter/builtin_functions.cc

Debug_Obj=$(addprefix bin/$(os)/debug/,$(Obj)) bin/$(os)/debug/builtin_function_documentation.o

bin/$(os)/debug/$(Target): $(Debug_Obj) bin/$(os)/debug/main.o bin/$(os)/rtmidi.o $(Bestline)
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $(Debug_Obj) bin/$(os)/rtmidi.o $(Bestline) $(LDFLAGS) $(LDLIBS)

bin/$(os)/debug/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $< -c


bin/$(os)/debug/builtin_function_documentation.o: bin/$(os)/builtin_function_documentation.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $< -c

# http://www.music.mcgill.ca/~gary/rtmidi/#compiling
bin/$(os)/rtmidi.o: lib/rtmidi/RtMidi.cpp lib/rtmidi/RtMidi.h
	@echo "CXX $@"
	@$(CXX) $< -c -O2 -o $@ $(CPPFLAGS) -std=c++20


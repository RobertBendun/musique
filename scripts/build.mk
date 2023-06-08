TARGET_OBJ = $(addprefix $(PREFIX)/,$(Obj)) $(PREFIX)/builtin_function_documentation.o

$(PREFIX)/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $< -c

$(PREFIX)/$(Target): $(TARGET_OBJ) $(MAIN) $(PREFIX)/rtmidi.o
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $(shell CXX=$(CXX) os=$(os) scripts/build_replxx.sh) $^ $(LDFLAGS) $(LDLIBS)

$(PREFIX)/builtin_function_documentation.o: $(PREFIX)/builtin_function_documentation.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ $< -c

$(PREFIX)/builtin_function_documentation.cc: musique/interpreter/builtin_functions.cc scripts/document-builtin.py
	scripts/document-builtin.py -f cpp -o $@ musique/interpreter/builtin_functions.cc

# http://www.music.mcgill.ca/~gary/rtmidi/#compiling
$(PREFIX)/rtmidi.o: lib/rtmidi/RtMidi.cpp lib/rtmidi/RtMidi.h
	@echo "CXX $@"
	@$(CXX) $< -c -O2 -o $@ $(CPPFLAGS) -std=c++20

Release_Obj=$(addprefix bin/$(os)/,$(Obj))

# http://www.music.mcgill.ca/~gary/rtmidi/#compiling
bin/$(os)/rtmidi.o: lib/rtmidi/RtMidi.cpp lib/rtmidi/RtMidi.h
	@echo "CXX $@"
	@$(CXX) $< -c -O2 -o $@ $(CPPFLAGS) -std=c++20

bin/$(os)/serial/serial.o: lib/serial/src/serial.cc
	$(CXX) $^ -c -O2 -o $@ $(CPPFLAGS) -std=c++20

bin/$(os)/serial/$(Serial_Impl).o: lib/serial/src/impl/$(Serial_Impl).cc
	$(CXX) $^ -c -O2 -o $@ $(CPPFLAGS) -std=c++20

bin/$(os)/serial/$(Serial_List_Ports).o: lib/serial/src/impl/list_ports/$(Serial_List_Ports).cc
	$(CXX) $^ -c -O2 -o $@ $(CPPFLAGS) -std=c++20

Serial_Obj=bin/$(os)/serial/serial.o bin/$(os)/serial/$(Serial_Impl).o bin/$(os)/serial/$(Serial_List_Ports).o

bin/$(os)/bestline.o: lib/bestline/bestline.c lib/bestline/bestline.h
	@echo "CC $@"
	@$(CC) $< -c -O3 -o $@

bin/$(os)/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $< -c

bin/$(os)/$(Target): $(Release_Obj) bin/$(os)/main.o bin/$(os)/rtmidi.o $(Bestline) $(Serial_Obj)
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $(Release_Obj) bin/$(os)/rtmidi.o $(Bestline) $(Serial_Obj) $(LDFLAGS) $(LDLIBS)

Debug_Obj=$(addprefix bin/$(os)/debug/,$(Obj))

bin/$(os)/debug/$(Target): $(Debug_Obj) bin/$(os)/debug/main.o bin/$(os)/rtmidi.o $(Bestline) $(Serial_Obj)
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $(Debug_Obj) bin/$(os)/rtmidi.o $(Bestline) $(Serial_Obj) $(LDFLAGS) $(LDLIBS)

bin/$(os)/debug/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $< -c


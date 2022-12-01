Release_Obj=$(addprefix bin/$(os)/,$(Obj))

bin/$(os)/bestline.o: lib/bestline/bestline.c lib/bestline/bestline.h
	@echo "CC $@"
	@$(CC) $< -c -O3 -o $@

bin/$(os)/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $< -c

bin/$(os)/$(Target): $(Release_Obj) bin/$(os)/main.o bin/$(os)/rtmidi.o $(Bestline) bin/linux/serial/serial.o bin/linux/serial/unix.o bin/linux/serial/list_ports.o
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $(Release_Obj) bin/$(os)/rtmidi.o $(Bestline) $(LDFLAGS) $(LDLIBS) bin/linux/serial/serial.o bin/linux/serial/unix.o bin/linux/serial/list_ports.o

Debug_Obj=$(addprefix bin/$(os)/debug/,$(Obj))

bin/$(os)/debug/$(Target): $(Debug_Obj) bin/$(os)/debug/main.o bin/$(os)/rtmidi.o $(Bestline) bin/linux/serial/serial.o bin/linux/serial/unix.o bin/linux/serial/list_ports.o
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $(Debug_Obj) bin/$(os)/rtmidi.o $(Bestline) $(LDFLAGS) $(LDLIBS) bin/linux/serial/serial.o bin/linux/serial/unix.o bin/linux/serial/list_ports.o

bin/$(os)/debug/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $< -c


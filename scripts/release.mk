Release_Obj=$(addprefix bin/,$(Obj))

bin/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $< -c

bin/musique: $(Release_Obj) bin/main.o bin/bestline.o lib/midi/libmidi-alsa.a
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $(Release_Obj) bin/bestline.o bin/main.o $(LDFLAGS) $(LDLIBS)

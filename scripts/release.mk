Release_Obj=$(addprefix bin/$(os)/,$(Obj))

bin/$(os)/bestline.o: lib/bestline/bestline.c lib/bestline/bestline.h
	@echo "CC $@"
	@$(CC) $< -c -O3 -o $@

bin/$(os)/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $< -c

bin/musique: $(Release_Obj) bin/$(os)/main.o bin/$(os)/rtmidi.o $(Bestline)
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $(Release_Obj) bin/$(os)/rtmidi.o $(Bestline) $(LDFLAGS) $(LDLIBS)

Debug_Obj=$(addprefix bin/debug/,$(Obj))

debug: bin/debug/musique

bin/debug/musique: $(Debug_Obj) bin/debug/main.o bin/bestline.o
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $(Debug_Obj) bin/bestline.o  $(LDFLAGS) $(LDLIBS)

bin/debug/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $< -c

Debug_Obj=$(addprefix bin/debug/,$(Obj))

debug: bin/debug/musique

bin/debug/musique: $(Debug_Obj) bin/debug/main.o bin/bestline.o include/*.hh
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $(Debug_Obj) bin/bestline.o bin/debug/main.o  $(LDFLAGS) $(LDLIBS)

bin/debug/%.o: src/%.cc include/*.hh
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $< -c

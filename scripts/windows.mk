ifeq ($(os),windows)
Release_Obj=$(addprefix bin/,$(Obj))

bin/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $< -c

bin/musique: $(Release_Obj) bin/main.o bin/rtmidi.o
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $(Release_Obj) bin/rtmidi.o $(LDFLAGS) $(LDLIBS)
endif

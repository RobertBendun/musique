Release_Obj=$(addprefix bin/$(os)/,$(Obj))

Server=bin/$(os)/server/server.h bin/$(os)/server/server.o

$(Server) &: server/src/*.go
	cd server/src/; GOOS="$(GOOS)" GOARCH="$(GOARCH)" CGO_ENABLED=1 CC="$(CC)" \
		go build -o ../../bin/$(os)/server/server.o -buildmode=c-archive

bin/$(os)/bestline.o: lib/bestline/bestline.c lib/bestline/bestline.h
	@echo "CC $@"
	@$(CC) $< -c -O3 -o $@

bin/$(os)/%.o: musique/%.cc $(Server)
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $< -c

bin/$(os)/$(Target): $(Release_Obj) bin/$(os)/main.o bin/$(os)/rtmidi.o $(Bestline) $(Server)
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $(CPPFLAGS) -o $@ $(Release_Obj) bin/$(os)/rtmidi.o $(Bestline) $(LDFLAGS) $(LDLIBS) bin/$(os)/server/server.o

Debug_Obj=$(addprefix bin/$(os)/debug/,$(Obj))

bin/$(os)/debug/$(Target): $(Debug_Obj) bin/$(os)/debug/main.o bin/$(os)/rtmidi.o $(Bestline)
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $(Debug_Obj) bin/$(os)/rtmidi.o $(Bestline) $(LDFLAGS) $(LDLIBS)

bin/$(os)/debug/%.o: musique/%.cc
	@echo "CXX $@"
	@$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(CPPFLAGS) -o $@ $< -c


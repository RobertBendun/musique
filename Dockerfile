FROM ubuntu:20.04 AS linux
ARG TZ=Europe/Warsaw
ARG VERSION
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
RUN apt update && apt install -y make software-properties-common zip unzip git
RUN add-apt-repository ppa:ubuntu-toolchain-r/test
RUN apt install -y gcc-11 g++-11 libasound2-dev
RUN mkdir -p /src/
WORKDIR /src/
COPY config.mk Makefile /src/
COPY .git /src/.git/
COPY musique /src/musique/
COPY lib /src/lib/
COPY scripts /src/scripts/
RUN make clean && make CC=gcc-11 CXX=g++-11 VERSION="$VERSION"


FROM ubuntu:22.04 AS windows
ARG VERSION
RUN apt update && apt install -y git make gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64
RUN mkdir -p /src/
WORKDIR /src/
COPY config.mk Makefile /src/
COPY .git /src/.git/
COPY musique /src/musique/
COPY lib /src/lib/
COPY scripts /src/scripts/
RUN make clean && make os=windows CC=x86_64-w64-mingw32-gcc-posix CXX=x86_64-w64-mingw32-g++-posix VERSION="$VERSION"


FROM ubuntu:22.04 AS release
RUN apt update && apt install -y zip pandoc python3 python-is-python3 pandoc
COPY CHANGELOG.md LICENSE /musique/
COPY examples /musique/examples/
COPY --from=windows /src/bin/musique.exe /musique/musique-windows.exe
COPY --from=linux /src/bin/musique /musique/musique-linux

COPY doc /doc/
COPY scripts /scripts/
RUN python /scripts/language-cmp-cheatsheet.py /doc/musique-vs-languages-cheatsheet.template \
	&& bash -c 'cp /{doc,musique}/musique-vs-languages-cheatsheet.html'
RUN pandoc -o /musique/wprowadzenie.html /doc/wprowadzenie.md -s --toc
RUN zip -r musique.zip /musique/

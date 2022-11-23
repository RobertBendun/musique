package main

import (
	"fmt"
	"net/http"
)

func cmdHandler(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path != "/cmd" {
		http.Error(w, "404 not found.", http.StatusNotFound)
		return
	}

	if r.Method != "POST" {
		http.Error(w, "Method is not supported.", http.StatusNotFound)
		return
	}

	if err := r.ParseForm(); err != nil {
		fmt.Fprintf(w, "ParseForm() err: %v", err)
		return
	}
	fmt.Fprintf(w, "POST request successful\n")
	cmd := r.FormValue("cmd")
	fmt.Fprintf(w, "Command = %s\n", cmd)
}

func helloHandler(w http.ResponseWriter, r *http.Request) {
	if r.URL.Path != "/hello" {
		http.Error(w, "404 not found.", http.StatusNotFound)
		return
	}

	if r.Method != "GET" {
		http.Error(w, "Method is not supported.", http.StatusNotFound)
		return
	}

	fmt.Fprintf(w, "Hello!")
}

func scanHandler(w http.ResponseWriter, r *http.Request) {

}

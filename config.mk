NAME:=cluterm
VERSION:=0.1.0
BUILD:=.build
ROOT:=$(shell pwd)

DEFINE:=-DNAME='"$(NAME)"' -DVERSION='"$(VERSION)"' -D_XOPEN_SOURCE=700
FLAGS:=-Wall -Wextra -Wvla -pedantic -std=c99

NAME:=cluterm
VERSION:=0.1.0
BUILD:=.build
ROOT:=$(shell pwd)

DEFINE:=-DNAME='"$(NAME)"' -DVERSION='"$(VERSION)"'
FLAGS:=-Wall -Wextra -Wvla -pedantic -std=c11


.PHONY: all clean check check-ulmtest check-ulmprobe

all: check

clean:
	cd ulmtest; $(MAKE) clean
	cd ulmprobe; $(MAKE) clean

check: check-ulmtest

check-ulmtest:
	@echo '============================================================'
	@echo '                    Begin test: ulmtest                     '
	@echo '============================================================'
	@cd ulmtest; $(MAKE) check
	@echo '============================================================'
	@echo '                     End test: ulmtest                      '
	@echo '============================================================'

check-ulmprobe:
	@echo '============================================================'
	@echo '                    Begin test: ulmprobe
	@echo '============================================================'
	@cd ulmprobe; $(MAKE) check
	@echo '============================================================'
	@echo '                     End test: ulmprobe                     '
	@echo '============================================================'

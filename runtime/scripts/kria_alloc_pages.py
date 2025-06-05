print("""
      This script will attempt to change the number of Linux HugePages allocated by the OS.
      It may fail, be sure to check how many are allocated afterwards to make sure there are
      enough.
      """)

print("Currently allocated: #free/#total")
hugepage_sizes=[64, 2048, 32768, 1048576]

for i, sz in enumerate(hugepage_sizes):
    with open(f"/sys/kernel/mm/hugepages/hugepages-{sz}kB/nr_hugepages") as f1:
        nr_huge = int(f1.read().strip())
    with open(f"/sys/kernel/mm/hugepages/hugepages-{sz}kB/free_hugepages") as f1:
        free_huge = int(f1.read().strip())
    print(f"[{i}] - {sz}kB: {free_huge}/{nr_huge}")

print("Enter the hugepage size you would like to allocate for (blank for quit):")
hp_id = input()
if hp_id == "":
    exit(0)

print("Enter the number of hugepages you would like to allocate:")
hp_n = input()
if int(hp_n) < 0:
    print("Cannot allocate less than 0")
    exit(1)

with open(f"/sys/kernel/mm/hugepages/hugepages-{hugepage_sizes[int(hp_id)]}kB/nr_hugepages", 'w') as f1:
    f1.write(hp_n)

print("Attempted to allocate. Do run this script again to make sure the changes persisted (success).")


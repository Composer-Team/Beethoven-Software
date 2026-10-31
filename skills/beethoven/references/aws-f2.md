# AWS F2 flow (`aws-f2`)

Completely different from Zynq: **do not use `beethoven synth` or
`beethoven flash`.** Synthesis happens on an AWS compile VM via AWS's own
build scripts; the result becomes an Amazon FPGA Image (AFI, global id AGFI)
that is loaded onto an F2 instance.

Three machines:

| Machine | Role | Setup |
|---|---|---|
| **Local** dev machine | write/simulate design, generate the CL package | full setup (setup.md) |
| **Compile VM** (EC2, FPGA Developer AMI with Vivado) | build the design checkpoint (~1.5 h), create the AFI | deploy-only + `tmux python3` |
| **F2 instance** (e.g. `f2.6xlarge`) | load the AGFI, run user program + runtime | deploy-only + aws-fpga SDK |

Both EC2 machines cost money while running: remind the user to stop or
terminate them when done. Launching/stopping instances and AWS credentials
are the user's decisions — confirm before running any `aws ec2` command that
creates, starts, or terminates resources.

## 0. Prerequisites (user)

- AWS CLI v2 installed and `aws configure` done by the user (access key,
  secret, region e.g. `us-east-1`, output `json`). Never handle the secret
  yourself. Check with `aws sts get-caller-identity`.
- SSH key (`.pem`) for both EC2 machines; `chmod 600` it.
- Compile VM public IP (`$VM`) and F2 instance id/IP. To start an existing F2
  instance and get its IP:
  ```bash
  aws ec2 start-instances --instance-ids <id> --region <r>
  aws ec2 wait instance-running --instance-ids <id> --region <r>
  aws ec2 describe-instances --instance-ids <id> --region <r> \
    --query 'Reservations[0].Instances[0].PublicIpAddress' --output text
  ```

Run remote commands as `ssh -i <key.pem> ubuntu@<ip> '<cmd>'`. Non-login SSH
shells do not load Vivado or other profile settings — source them explicitly.

## 1. Local: configure and generate the CL package

```toml
[platform]
target = "aws-f2"

[platform.aws-f2]
# remote-username = "ubuntu"
```

Simulate first (`beethoven build --simulation && timeout 900 beethoven sim`).
Then:

```bash
source "$HOME/.sdkman/bin/sdkman-init.sh" && beethoven build hw --release
```

This emits the custom-logic (CL) package at
`target/synthesis/aws/cl_beethoven_top/` (including `build_beethoven_f2.sh`).

## 2. Local: upload to the compile VM

```bash
beethoven aws upload --host ubuntu@$VM --key /path/to/key.pem
#   [--remote-dir ~/cl_beethoven_top] [--delete]
```

Upload only (rsync); it does not start the build.

## 3. Compile VM: one-time setup

On the VM: install the x86_64 CLI binary, then
`sudo apt-get update && sudo apt-get install -y cmake build-essential git curl tmux python3`,
then `beethoven setup --no-hardware` (setup.md → Deploy-only machines).

`create-fpga-image` needs AWS credentials on the VM. The user copies them
(from their local shell):

```bash
ssh -i key.pem ubuntu@$VM 'mkdir -p ~/.aws && chmod 700 ~/.aws'
scp -i key.pem ~/.aws/credentials ~/.aws/config ubuntu@$VM:~/.aws/
ssh -i key.pem ubuntu@$VM 'chmod 600 ~/.aws/* && aws sts get-caller-identity'
```

## 4. Compile VM: build the checkpoint (~1.5 h)

Run inside a detached `tmux` so it survives SSH disconnects; the wrapper
clones `aws-fpga` (branch `f2`) into `~/aws-fpga` if absent (override with
`BEETHOVEN_AWS_FPGA_REPO_DIR`), sources `hdk_setup.sh`, and runs AWS's DCP
build. Vivado's path depends on the AMI (`/opt/Xilinx/2025.2/...` on the
current FPGA Developer AMI):

```bash
ssh -i key.pem ubuntu@$VM "tmux new -d -s f2-build \
  'cd ~/cl_beethoven_top && source /opt/Xilinx/2025.2/Vivado/settings64.sh && ./build_beethoven_f2.sh 2>&1 | tee build.log'"
# poll occasionally (not in a tight loop):
ssh -i key.pem ubuntu@$VM 'tail -n 5 ~/cl_beethoven_top/build.log; tmux has-session -t f2-build 2>/dev/null && echo RUNNING || echo DONE'
```

## 5. Compile VM: create the FPGA image

```bash
ssh -i key.pem ubuntu@$VM 'cd ~/cl_beethoven_top && \
  beethoven aws create-fpga-image --name <image-name> --region <region> --yes --auto'
```

It validates post-route timing, uploads the checkpoint tarball to S3
(`--bucket` to choose; otherwise a generated bucket), and calls
`aws ec2 create-fpga-image`. Always pass `--name --region --yes --auto` (no
TTY). `--dry-run` prints the plan. **Record the AFI and AGFI ids it prints.**
The image takes a while to become usable:

```bash
aws ec2 describe-fpga-images --owners self --region <region> \
  --filters Name=name,Values=<image-name> \
  --query 'FpgaImages[0].{State:State.Code,AFI:FpgaImageId,AGFI:FpgaImageGlobalId,Msg:State.Message}' --output json
```

Wait for `State` = `available`. Then the compile VM is no longer needed —
remind the user to terminate it (and delete any temporary security group).

## 6. F2 instance: one-time setup

On the F2 instance:

1. x86_64 CLI binary; packages
   `sudo apt-get update && sudo apt-get install -y cmake build-essential git curl python3 rsync unzip`;
   `beethoven setup --no-hardware`.
2. AWS FPGA SDK — needed for `aws load` (`fpga-load-local-image`) and for the
   release runtime build (`AWS_FPGA_REPO_DIR`):
   ```bash
   [ -d ~/aws-fpga ] || git clone --branch f2 https://github.com/aws/aws-fpga.git ~/aws-fpga
   cd ~/aws-fpga && source ./sdk_setup.sh && cd -
   ```
   **`sdk_setup.sh` must be sourced from the `aws-fpga` root**, otherwise it
   fails with `not a git repository` / `AWS_FPGA_REPO_DIR=unset`. Each fresh
   agent shell must source it again in the same command as the step needing it.

## 7. Copy the project to the F2 instance

From the local machine (exclude local binaries, whose library paths point at
the local home directory):

```bash
rsync -az -e "ssh -i key.pem" \
  --exclude=/target/sw/ --exclude=/target/synthesis/runtime/ \
  --exclude=/target/simulation/ --exclude=/target/synthesis/aws/ \
  ./ ubuntu@$F2:~/my-design/
```

If an old copy exists on F2, ask before replacing it.

## 8. F2 instance: load the image

```bash
cd ~/aws-fpga && source ./sdk_setup.sh && cd ~/my-design && \
sudo -E env PATH="$PATH" beethoven aws load --agfi agfi-XXXXXXXX --slot 0
```

With `--agfi`, no AWS API call is needed. Alternatives: `--afi afi-...` or
`--name <image-name>` (these query AWS; add `--region` and `--yes`). Success
shows the slot as `loaded ... ok`. `aws load` neither builds nor starts the
runtime.

## 9. F2 instance: build and run user program + runtime

Build as the normal user, run under sudo (the runtime maps `/dev/mem`),
carrying the SDK environment through sudo:

```bash
cd ~/aws-fpga && source ./sdk_setup.sh && cd ~/my-design && \
beethoven build runtime --release && beethoven build sw && \
sudo -E env HOME="$HOME" PATH="$PATH" AWS_FPGA_REPO_DIR="$AWS_FPGA_REPO_DIR" \
  LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}" timeout 600 beethoven run --no-build [TESTBENCH] [-- <args>]
```

The generated template testbench prints `[PASS] ...` on success.

When finished, remind the user to stop the F2 instance
(`aws ec2 stop-instances --instance-ids <id> --region <r>`).

Failures: troubleshooting.md (`/dev/mem` permission, SDK env, hangs,
`runtime kill` side effects).

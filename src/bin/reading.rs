use std::env;
use std::fs::{metadata, File};
use std::io::prelude::*;
use std::os::unix::io::AsRawFd;
use std::path::{Path, PathBuf};
use std::result::Result;

struct Options {
    file: PathBuf,
}
impl Options {
    fn parse() -> Result<Options, String> {
        let args: Vec<String> = env::args().collect();
        match args.len() {
            2 => Ok(Options {
                file: PathBuf::from(args[1].clone()),
            }),
            _ => Err("Invalid number of arguments".to_string()),
        }
    }
}

#[allow(dead_code)]
fn read(path: &Path) -> u64 {
    let mut buffer = [0u8; 256 * 1024];
    let mut bytes = 0u64;
    let mut fd = File::open(path).expect("Unable to open file");
    let mut token = 0u64;

    loop {
        let r = fd.read(&mut buffer).expect("Unable to read file");
        bytes += r as u64;
        if r == 0 {
            break;
        }
        let mut pointer = buffer.as_ptr();
        let end = unsafe { pointer.add(bytes as usize) };
        while pointer < end {
            token += unsafe { *(pointer as *const u64) };
            pointer = unsafe { pointer.add(8) };
        }
    }

    println!("Token: {}", token);
    bytes
}

#[allow(dead_code)]
fn read_mmap(path: &Path) -> u64 {
    let fd = File::open(path).expect("Unable to open file");
    let meta = metadata(path).expect("unable to get metadata");

    let mem = unsafe {
        libc::mmap(
            std::ptr::null_mut(),
            meta.len() as usize,
            libc::PROT_READ,
            libc::MAP_SHARED,
            fd.as_raw_fd(),
            0,
        )
    };

    if mem == libc::MAP_FAILED {
        panic!("Unable to map file.");
    }

    let mut pointer = mem as *const libc::c_char;
    let end = unsafe { pointer.add(meta.len() as usize - 8) };
    let mut token = 0u64;

    while pointer < end {
        let piece = unsafe { *(pointer as *const u64) };
        token += piece;
        pointer = unsafe { pointer.add(8) };
    }

    println!("Token signature: {}", token);
    meta.len()
}

fn main() {
    let options = match Options::parse() {
        Ok(opts) => opts,
        Err(msg) => {
            eprintln!("ERROR: unable to parse arguments: {}", msg);
            std::process::exit(1);
        }
    };

    // let bytes = read_mmap(&options.file);
    let bytes = read(&options.file);
    println!("Read {:#?} bytes from file.", bytes);
}

use std::env;
use std::fs;
use std::io::{Read, Write};
use std::net::{TcpListener, TcpStream};
use std::path::{Component, Path, PathBuf};
use std::thread;

#[derive(Clone)]
struct Options {
    port: u16,
    root: PathBuf,
}

fn main() -> std::io::Result<()> {
    let options = parse_args();
    let listener = TcpListener::bind(("0.0.0.0", options.port))?;
    println!(
        "Rust TinyWebServer listening on http://127.0.0.1:{}/ serving {}",
        options.port,
        options.root.display()
    );

    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                let root = options.root.clone();
                thread::spawn(move || handle_client(stream, root));
            }
            Err(err) => eprintln!("accept: {err}"),
        }
    }
    Ok(())
}

fn parse_args() -> Options {
    let mut port = 9006;
    let mut root = PathBuf::from("root");
    let mut args = env::args().skip(1);

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--port" | "-p" => {
                port = args
                    .next()
                    .expect("--port requires a value")
                    .parse()
                    .expect("--port must be a number");
            }
            "--root" | "-r" => root = PathBuf::from(args.next().expect("--root requires a value")),
            _ => {
                eprintln!("usage: tinywebserver-rust [--port PORT] [--root DIR]");
                std::process::exit(2);
            }
        }
    }

    Options {
        port,
        root: fs::canonicalize(root).expect("static root must exist"),
    }
}

fn handle_client(mut stream: TcpStream, root: PathBuf) {
    let mut buffer = [0_u8; 4096];
    let Ok(bytes) = stream.read(&mut buffer) else {
        return;
    };
    if bytes == 0 {
        return;
    }

    let request = String::from_utf8_lossy(&buffer[..bytes]);
    let mut parts = request.lines().next().unwrap_or_default().split_whitespace();
    let method = parts.next().unwrap_or_default();
    let mut uri = parts.next().unwrap_or_default().to_string();

    if method.is_empty() || uri.is_empty() {
        send_error(&mut stream, 400);
        return;
    }
    if method != "GET" && method != "HEAD" {
        send_error(&mut stream, 405);
        return;
    }

    if let Some(index) = uri.find('?') {
        uri.truncate(index);
    }
    if uri == "/" {
        uri = "/welcome.html".to_string();
    }

    let Ok(path) = safe_path(&root, &uri) else {
        send_error(&mut stream, 403);
        return;
    };

    let Ok(metadata) = fs::metadata(&path) else {
        send_error(&mut stream, 404);
        return;
    };
    if !metadata.is_file() {
        send_error(&mut stream, 404);
        return;
    }

    let header = format!(
        "HTTP/1.1 200 OK\r\nContent-Type: {}\r\nContent-Length: {}\r\nConnection: close\r\n\r\n",
        content_type(&path),
        metadata.len()
    );
    if stream.write_all(header.as_bytes()).is_err() || method == "HEAD" {
        return;
    }
    if let Ok(mut file) = fs::File::open(path) {
        let _ = std::io::copy(&mut file, &mut stream);
    }
}

fn safe_path(root: &Path, uri: &str) -> std::io::Result<PathBuf> {
    let mut path = root.to_path_buf();
    for component in Path::new(uri.trim_start_matches('/')).components() {
        match component {
            Component::Normal(part) => path.push(part),
            Component::CurDir => {}
            _ => {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::PermissionDenied,
                    "path escapes root",
                ));
            }
        }
    }
    Ok(path)
}

fn send_error(stream: &mut TcpStream, status: u16) {
    let reason = match status {
        400 => "Bad Request",
        403 => "Forbidden",
        404 => "Not Found",
        405 => "Method Not Allowed",
        _ => "Internal Server Error",
    };
    let body = format!("<html><body><h1>{status} {reason}</h1></body></html>");
    let response = format!(
        "HTTP/1.1 {status} {reason}\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{body}",
        body.len()
    );
    let _ = stream.write_all(response.as_bytes());
}

fn content_type(path: &Path) -> &'static str {
    match path.extension().and_then(|ext| ext.to_str()).unwrap_or_default() {
        "html" | "htm" => "text/html; charset=utf-8",
        "css" => "text/css; charset=utf-8",
        "js" => "application/javascript; charset=utf-8",
        "jpg" | "jpeg" => "image/jpeg",
        "png" => "image/png",
        "gif" => "image/gif",
        "ico" => "image/x-icon",
        "mp4" => "video/mp4",
        _ => "application/octet-stream",
    }
}

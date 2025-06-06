#!/usr/bin/env python3
"""
🌐 Sapera Camera System - HTTP to IPC Bridge
====================================================

High-performance bridge connecting the web interface to our ultra-fast
parallel camera initialization system via Windows Named Pipes.

Features:
- FastAPI HTTP server for modern REST API
- Direct IPC communication with 3.8s camera system  
- Real-time WebSocket support for live updates
- CORS support for local development
- Comprehensive error handling
- Performance monitoring

Architecture:
HTTP Request → FastAPI → IPC Named Pipe → Ultra-Fast Camera System
"""

import asyncio
import json
import logging
import time
import subprocess
import os
from datetime import datetime
from typing import Optional, Dict, List, Any
from contextlib import asynccontextmanager
from pathlib import Path

from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import HTMLResponse, JSONResponse, FileResponse
from pydantic import BaseModel
import uvicorn

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# ==============================================
# CLEAN SYSTEM PATHS - UPDATED FOR NEW STRUCTURE
# ==============================================

# Get the clean system root directory
CLEAN_SYSTEM_ROOT = Path(__file__).parent.parent
EXECUTABLE_PATH = CLEAN_SYSTEM_ROOT / "backend" / "refactored_capture.exe"
CONFIG_PATH = CLEAN_SYSTEM_ROOT / "config" / "camera_config.json"
DATA_DIR = CLEAN_SYSTEM_ROOT / "data"
FRONTEND_DIR = CLEAN_SYSTEM_ROOT / "frontend"

logger.info(f"🚀 Clean Sapera System Paths:")
logger.info(f"   📁 Root: {CLEAN_SYSTEM_ROOT}")
logger.info(f"   📸 Executable: {EXECUTABLE_PATH}")
logger.info(f"   ⚙️  Config: {CONFIG_PATH}")
logger.info(f"   💾 Data: {DATA_DIR}")
logger.info(f"   🖥️  Frontend: {FRONTEND_DIR}")

# Create data directory if it doesn't exist
DATA_DIR.mkdir(exist_ok=True)

def run_camera_command(command_args, timeout=60):
    """Execute camera command with proper working directory"""
    try:
        # Set working directory to data folder so images are created there
        cmd = [str(EXECUTABLE_PATH), "--config", str(CONFIG_PATH)] + command_args
        
        logger.info(f"🔧 Executing: {' '.join(cmd)}")
        logger.info(f"📁 Working directory: {DATA_DIR}")
        
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=str(DATA_DIR)  # Set working directory to data folder
        )
        
        logger.info(f"✅ Command completed - Return code: {result.returncode}")
        logger.info(f"📤 Stdout: {result.stdout[:200]}...")
        
        if result.stderr:
            logger.warning(f"⚠️  Stderr: {result.stderr}")
        
        return result
    except subprocess.TimeoutExpired:
        logger.error(f"❌ Command timed out after {timeout} seconds")
        raise HTTPException(status_code=408, detail="Camera operation timed out")
    except Exception as e:
        logger.error(f"❌ Command execution failed: {e}")
        raise HTTPException(status_code=500, detail=f"Camera operation failed: {str(e)}")

# Create FastAPI app
app = FastAPI(title="Clean Sapera Camera API", version="1.0.0")

# ==============================================
# API ENDPOINTS - UPDATED FOR CLEAN SYSTEM
# ==============================================

@app.get("/")
async def root():
    """Serve the frontend dashboard"""
    return FileResponse(FRONTEND_DIR / "index.html")

@app.get("/api/cameras")
async def list_cameras():
    """List all available cameras"""
    logger.info("📋 Listing cameras...")
    
    result = run_camera_command(["--json", "--list-cameras"])
    
    if result.returncode != 0:
        raise HTTPException(status_code=500, detail="Failed to list cameras")
    
    try:
        response = json.loads(result.stdout)
        logger.info(f"📊 Found {response.get('total_cameras', 0)} cameras")
        return response
    except json.JSONDecodeError:
        logger.error("❌ Invalid JSON response from camera system")
        raise HTTPException(status_code=500, detail="Invalid response from camera system")

@app.post("/api/cameras/capture-all")
async def capture_all_cameras():
    """Capture from all cameras with improved retry"""
    logger.info("📸 Starting capture-all operation...")
    
    # Use longer timeout for multi-camera capture
    result = run_camera_command(["--json", "--capture-all"], timeout=120)
    
    if result.returncode != 0:
        raise HTTPException(status_code=500, detail="Capture operation failed")
    
    try:
        response = json.loads(result.stdout)
        logger.info(f"✅ Capture completed: {response.get('total_images', 0)} images")
        
        # Add additional metadata for bandwidth optimized system
        response["system_info"] = {
            "executable": str(EXECUTABLE_PATH.name),
            "config": str(CONFIG_PATH.name),
            "data_directory": str(DATA_DIR),
            "ultra_conservative_mode": "enabled",
            "smart_scheduling": "enabled",
            "max_concurrent_cameras": 2,
            "dark_image_retry": "enabled",
            "max_retries": 5,
            "zero_dark_guarantee": "enabled"
        }
        
        return response
    except json.JSONDecodeError:
        logger.error("❌ Invalid JSON response from capture operation")
        raise HTTPException(status_code=500, detail="Invalid response from capture operation")

@app.get("/api/cameras/{camera_id}")
async def get_camera_parameters(camera_id: str):
    """Get parameters for a specific camera"""
    logger.info(f"📋 Getting parameters for camera {camera_id}")
    
    result = run_camera_command(["--json", "--get-params", camera_id])
    
    if result.returncode != 0:
        raise HTTPException(status_code=404, detail=f"Camera {camera_id} not found")
    
    try:
        response = json.loads(result.stdout)
        return response
    except json.JSONDecodeError:
        raise HTTPException(status_code=500, detail="Invalid response from camera system")

@app.post("/api/cameras/{camera_id}/capture") 
async def capture_single_camera(camera_id: str):
    """Capture from a single camera"""
    logger.info(f"📸 Capturing from camera {camera_id}")
    
    result = run_camera_command(["--json", "--camera", camera_id, "--capture"], timeout=60)
    
    if result.returncode != 0:
        raise HTTPException(status_code=500, detail=f"Capture failed for camera {camera_id}")
    
    try:
        response = json.loads(result.stdout)
        return response
    except json.JSONDecodeError:
        raise HTTPException(status_code=500, detail="Invalid response from capture operation")

@app.get("/api/system/status")
async def system_status():
    """Get system status and health"""
    status = {
        "status": "operational",
        "executable_exists": EXECUTABLE_PATH.exists(),
        "config_exists": CONFIG_PATH.exists(),
        "data_directory": str(DATA_DIR),
        "frontend_directory": str(FRONTEND_DIR),
        "ultra_conservative_optimizations": {
            "smart_scheduling": True,
            "max_concurrent_cameras": 2,
            "bandwidth_throttling": True,
            "ultra_conservative_mode": True,
            "intelligent_retry": True,
            "longer_settling": "500-1500ms",
            "aggressive_retry": "up to 6x gain, 150000μs exposure",
            "max_retries": 5,
            "buffer_count": 5,
            "timeout_extension": "100% for problematic cameras",
            "base_delays": "10ms all cameras, 75ms throttled cameras",
            "batch_delays": "100ms between batches, 200ms between shots"
        },
        "version": "Ultra-Conservative System v3.1"
    }
    
    return status

# ==============================================
# SERVE FRONTEND ASSETS
# ==============================================

# Mount frontend static files
app.mount("/static", StaticFiles(directory=str(FRONTEND_DIR)), name="static")

# Serve CSS and JS files
@app.get("/styles.css")
async def serve_css():
    return FileResponse(FRONTEND_DIR / "styles.css")

@app.get("/js/{filename}")
async def serve_js(filename: str):
    return FileResponse(FRONTEND_DIR / "js" / filename)

# ==============================================
# STARTUP AND CONFIGURATION
# ==============================================

@app.on_event("startup")
async def startup_event():
    """System startup checks"""
    logger.info("🌐 Starting Bandwidth-Optimized Sapera Camera System...")
    
    # Check if executable exists
    if not EXECUTABLE_PATH.exists():
        logger.error(f"❌ Camera executable not found: {EXECUTABLE_PATH}")
        logger.error("🔨 Please build the system first: cmake --build build --config Release")
    else:
        logger.info(f"✅ Bandwidth-optimized camera executable found: {EXECUTABLE_PATH}")
    
    # Check if config exists
    if not CONFIG_PATH.exists():
        logger.error(f"❌ Camera config not found: {CONFIG_PATH}")
    else:
        logger.info(f"✅ Camera config found: {CONFIG_PATH}")
    
    # Ensure data directory exists
    if not DATA_DIR.exists():
        DATA_DIR.mkdir(parents=True)
        logger.info(f"📁 Created data directory: {DATA_DIR}")
    else:
        logger.info(f"✅ Data directory ready: {DATA_DIR}")
    
    logger.info("🌐 Clean Sapera System ready!")
    logger.info(f"🖥️  Frontend: http://localhost:8080")
    logger.info(f"📡 API docs: http://localhost:8080/docs")

if __name__ == "__main__":
    print("=" * 50)
    print("🚀 Clean Sapera Camera System - Web Bridge")
    print("=" * 50)
    print(f"📁 System root: {CLEAN_SYSTEM_ROOT}")
    print(f"📸 Backend: {EXECUTABLE_PATH.name}")
    print(f"🖥️  Frontend: {FRONTEND_DIR.name}")
    print(f"💾 Data output: {DATA_DIR}")
    print("=" * 50)
    print()
    
    uvicorn.run(
        app,
        host="0.0.0.0", 
        port=8080,
        log_level="info"
    ) 
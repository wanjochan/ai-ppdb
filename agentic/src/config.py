import json
import os
from pathlib import Path
from typing import Optional, Dict
import logging

logger = logging.getLogger(__name__)

class Config:
    def __init__(self):
        self.base_dir = Path(__file__).resolve().parent.parent
        self.secrets_dir = self.base_dir / '.secrets'
        self.secrets_file = self.secrets_dir / 'config.json'
        
        # 确保secrets目录存在
        self.secrets_dir.mkdir(exist_ok=True)
        
        # 内存中的配置
        self.current_config: Dict = {}
        
    def save_local(self, config: Dict) -> None:
        """保存配置到本地文件"""
        try:
            # 确保不保存空API key
            if not config.get('apiKey'):
                return
                
            # 简单加密（实际项目中应使用更安全的加密方式）
            config['apiKey'] = self._simple_encrypt(config['apiKey'])
            
            with open(self.secrets_file, 'w', encoding='utf-8') as f:
                json.dump(config, f, ensure_ascii=False, indent=2)
                
            logger.info("配置已保存到本地")
        except Exception as e:
            logger.error(f"保存配置失败: {e}")
            
    def load_local(self) -> Optional[Dict]:
        """从本地文件加载配置"""
        try:
            if not self.secrets_file.exists():
                return None
                
            with open(self.secrets_file, 'r', encoding='utf-8') as f:
                config = json.load(f)
                
            # 解密API key
            if config.get('apiKey'):
                config['apiKey'] = self._simple_decrypt(config['apiKey'])
                
            return config
        except Exception as e:
            logger.error(f"加载配置失败: {e}")
            return None
            
    def _simple_encrypt(self, text: str) -> str:
        """简单的加密方法（示例用，实际应使用更安全的方式）"""
        return ''.join(chr(ord(c) + 1) for c in text)
        
    def _simple_decrypt(self, text: str) -> str:
        """简单的解密方法（示例用，实际应使用更安全的方式）"""
        return ''.join(chr(ord(c) - 1) for c in text)

# 全局配置实例
config = Config()

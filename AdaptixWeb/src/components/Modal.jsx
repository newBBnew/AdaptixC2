import React, { useEffect } from 'react';
import { X } from 'lucide-react';
import { motion, AnimatePresence } from 'framer-motion';
import { cn } from '../utils/cn';

const Modal = ({ isOpen, onClose, title, children, width = 'max-w-4xl', height = 'auto' }) => {
  useEffect(() => {
    const handleEsc = (e) => {
      if (e.key === 'Escape') onClose();
    };
    if (isOpen) window.addEventListener('keydown', handleEsc);
    return () => window.removeEventListener('keydown', handleEsc);
  }, [isOpen, onClose]);

  return (
    <AnimatePresence>
      {isOpen && (
        <div className="fixed inset-0 z-[100] flex items-center justify-center p-4 bg-theme-glass-panel/60 backdrop-blur-sm">
          <motion.div
            initial={{ opacity: 0, scale: 0.95, y: 20 }}
            animate={{ opacity: 1, scale: 1, y: 0 }}
            exit={{ opacity: 0, scale: 0.95, y: 20 }}
            className={cn(
              "bg-theme-glass-panel border border-theme-glass-light rounded-2xl shadow-glow-sm flex flex-col overflow-hidden",
              width,
              height === 'auto' ? 'max-h-[90vh]' : height
            )}
          >
            {/* Header */}
            <div className="flex items-center justify-between px-4 py-2 border-b border-theme-glass-light bg-theme-glass">
              <h3 className="text-[10px] font-black uppercase tracking-[0.2em] text-theme-muted">{title}</h3>
              <button
                onClick={onClose}
                className="p-1.5 hover:bg-theme-glass-panel rounded-xl transition-all text-theme-muted hover:text-theme-primary"
              >
                <X size={18} />
              </button>
            </div>

            {/* Content */}
            <div className="flex-1 overflow-auto custom-scrollbar">
              {children}
            </div>
          </motion.div>
        </div>
      )}
    </AnimatePresence>
  );
};

export default Modal;
